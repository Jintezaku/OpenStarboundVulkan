#include "StarTilePainter.hpp"
#include "StarLexicalCast.hpp"
#include "StarJsonExtra.hpp"
#include "StarXXHash.hpp"
#include "StarMaterialDatabase.hpp"
#include "StarLiquidsDatabase.hpp"
#include "StarAssets.hpp"
#include "StarRoot.hpp"
#include "StarTileDrawer.hpp"
#include "StarSet.hpp"
#include "StarTime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Star {

TilePainter::TilePainter(RendererPtr renderer) : TileDrawer() {
  m_renderer = std::move(renderer);
  m_textureGroup = m_renderer->createTextureGroup(TextureGroupSize::Large);

  auto& root = Root::singleton();
  auto assets = root.assets();

  m_reloadTracker = make_shared<TrackerListener>();
  root.registerReloadListener(m_reloadTracker);
  m_terrainDamageHashLevelQuantization = 16;
  m_liquidHashLevelQuantization = 32;
  m_enableAdaptiveChunkBuildBudget = true;
  m_baseTerrainChunkBuildBudgetMicros = 3000;
  m_baseLiquidChunkBuildBudgetMicros = 1500;
  m_minTerrainChunkBuildBudgetMicros = 500;
  m_minLiquidChunkBuildBudgetMicros = 250;
  m_chunkBuildBudgetOverloadStartMs = 6.0;
  m_chunkBuildBudgetOverloadMaxMs = 18.0;
  m_setupCostEmaMs = 0.0;
  m_setupCostSmoothing = 0.15;
  m_enableAdaptiveChunkHashCadence = true;
  m_chunkHashRefreshBaseStrideFrames = 1;
  m_chunkHashRefreshMaxStrideFrames = 4;
  m_chunkHashFarStrideFrames = 8;
  m_enableChunkHashRefreshBudget = true;
  m_chunkHashRefreshBudgetPerFrame = 48;
  m_enableAdaptiveChunkHashDebounce = true;
  m_chunkHashDebounceMismatchFrames = 2;
  m_chunkHashDebounceOverloadThreshold = 0.35;
  m_enableDistanceWeightedChunkHashCadence = true;
  m_enableVisibleChunkPriority = true;
  m_criticalChunkSyncBuildsPerFrame = 2;
  m_enableChunkPrefetch = true;
  m_enableAdaptiveChunkPrefetchCadence = true;
  m_chunkPrefetchRing = 1;
  m_chunkPrefetchPerFrame = 16;
  m_chunkPrefetchMinPerFrame = 2;
  m_setupFrameIndex = 0;
  refreshRenderConfig();

  for (auto const& liquid : root.liquidsDatabase()->allLiquidSettings()) {
    m_liquids.set(liquid->id, LiquidInfo{
        m_renderer->createTexture(*assets->image(liquid->config.getString("texture")), TextureAddressing::Wrap),
        jsonToColor(liquid->config.get("color")).toRgba(),
        jsonToColor(liquid->config.get("bottomLightMix")).toRgbF(),
        liquid->config.getFloat("textureMovementFactor")
      });
  }
}

void TilePainter::adjustLighting(WorldRenderData& renderData) const {
  RectI lightRange = RectI::withSize(renderData.lightMinPosition, Vec2I(renderData.lightMap.size()));
  forEachRenderTile(renderData, lightRange, [&](Vec2I const& pos, RenderTile const& tile) {
      // Only adjust lighting for tiles with liquid above the draw threshold
      float drawLevel = liquidDrawLevel(byteToFloat(tile.liquidLevel));
      if (drawLevel == 0.0f)
        return;

      auto lightIndex = Vec2U(pos - renderData.lightMinPosition);
      auto lightValue = renderData.lightMap.get(lightIndex.x(), lightIndex.y());

      auto const& liquid = m_liquids[tile.liquidId];
      float darknessLevel = (1.f - (lightValue.sum() / 3.0f)) * drawLevel;
      lightValue = lightValue.piecewiseMultiply(Vec3F::filled(1.f - darknessLevel) + liquid.bottomLightMix * darknessLevel);

      renderData.lightMap.set(lightIndex.x(), lightIndex.y(), lightValue);
    });
}

void TilePainter::setup(WorldCamera const& camera, WorldRenderData& renderData) {
  double setupStart = Time::monotonicTime();
  ++m_setupFrameIndex;

  bool worldGenerationChanged = !m_cachedWorldRenderGeneration
      || *m_cachedWorldRenderGeneration != renderData.worldRenderGeneration;
  if (worldGenerationChanged)
    m_cachedWorldRenderGeneration = renderData.worldRenderGeneration;

  auto worldSize = renderData.geometry.size();
  if (worldGenerationChanged || !m_cachedWorldSize || *m_cachedWorldSize != worldSize) {
    m_cachedWorldSize = worldSize;
    m_cachedChunkHashes.clear();
    m_lastTerrainChunks.clear();
    m_lastLiquidChunks.clear();
    m_terrainChunkCache.clear();
    m_liquidChunkCache.clear();
    m_lastCameraCenter = {};
    m_setupCostEmaMs = 0.0;
  }

  auto cameraCenter = camera.centerWorldPosition();
  if (m_lastCameraCenter)
    m_cameraPan = renderData.geometry.diff(cameraCenter, *m_lastCameraCenter);
  m_lastCameraCenter = cameraCenter;

  m_cameraTransformation = Mat3F::identity();
  m_cameraTransformation.translate(-Vec2F(camera.worldTileRect().min()));
  m_cameraTransformation.scale(TilePixels * camera.pixelRatio());
  m_cameraTransformation.translate(camera.tileMinScreen());

  //Kae: Padded by one to fix culling issues with certain tile pieces at chunk borders, such as grass.
  RectI chunkRange = RectI::integral(RectF(camera.worldTileRect().padded(1)).scaled(1.0f / RenderChunkSize));

  m_backgroundTerrainBuffers.clear();
  m_midgroundTerrainBuffers.clear();
  m_foregroundTerrainBuffers.clear();
  m_liquidBuffers.clear();

  Map<QuadZLevel, List<RenderBufferPtr>> backgroundBuffersByZ;
  Map<QuadZLevel, List<RenderBufferPtr>> midgroundBuffersByZ;
  Map<QuadZLevel, List<RenderBufferPtr>> foregroundBuffersByZ;

  double overload = 0.0;
  if (m_setupCostEmaMs > 0.0) {
    double overloadRange = std::max(0.1, m_chunkBuildBudgetOverloadMaxMs - m_chunkBuildBudgetOverloadStartMs);
    overload = std::clamp((m_setupCostEmaMs - m_chunkBuildBudgetOverloadStartMs) / overloadRange, 0.0, 1.0);
  }

  auto chunkBuildBudget = [&](int64_t baseBudgetMicros, int64_t minBudgetMicros) {
    if (!m_enableAdaptiveChunkBuildBudget || m_setupCostEmaMs <= 0.0)
      return baseBudgetMicros;

    double budgetScale = 1.0 - (overload * 0.75);
    int64_t scaledBudget = (int64_t)(baseBudgetMicros * budgetScale);
    return std::max(minBudgetMicros, scaledBudget);
  };

  int64_t terrainBudgetMicros = chunkBuildBudget(m_baseTerrainChunkBuildBudgetMicros, m_minTerrainChunkBuildBudgetMicros);
  int64_t liquidBudgetMicros = chunkBuildBudget(m_baseLiquidChunkBuildBudgetMicros, m_minLiquidChunkBuildBudgetMicros);
  if (worldGenerationChanged) {
    // World transitions must render fully-correct terrain immediately; avoid
    // adaptive throttling during this setup pass.
    terrainBudgetMicros = std::numeric_limits<int64_t>::max();
    liquidBudgetMicros = std::numeric_limits<int64_t>::max();
  }

  int hashRefreshStrideFrames = m_chunkHashRefreshBaseStrideFrames;
  if (m_enableAdaptiveChunkHashCadence) {
    int maxStride = std::max(m_chunkHashRefreshBaseStrideFrames, m_chunkHashRefreshMaxStrideFrames);
    hashRefreshStrideFrames = m_chunkHashRefreshBaseStrideFrames + (int)((maxStride - m_chunkHashRefreshBaseStrideFrames) * overload);
    hashRefreshStrideFrames = std::clamp(hashRefreshStrideFrames, 1, maxStride);
  }

  int hashRefreshBudgetRemaining = worldGenerationChanged
      ? std::numeric_limits<int>::max()
      : std::max(1, m_chunkHashRefreshBudgetPerFrame);

  Vec2I chunkCenter(
      (int)std::floor(cameraCenter[0] / (float)RenderChunkSize),
      (int)std::floor(cameraCenter[1] / (float)RenderChunkSize));

  auto chunkDistanceMetric = [&](Vec2I const& chunkIndex) {
    int dx = std::abs(chunkIndex[0] - chunkCenter[0]);
    int dy = std::abs(chunkIndex[1] - chunkCenter[1]);
    return std::make_pair(std::max(dx, dy), dx + dy);
  };

  auto localHashStride = [&](Vec2I const& chunkIndex) {
    int stride = hashRefreshStrideFrames;
    if (m_enableDistanceWeightedChunkHashCadence) {
      int dx = std::abs(chunkIndex[0] - chunkCenter[0]);
      int dy = std::abs(chunkIndex[1] - chunkCenter[1]);
      int ringDistance = std::max(dx, dy);
      if (ringDistance > 1) {
        int farStride = std::max(hashRefreshStrideFrames, m_chunkHashFarStrideFrames);
        stride = std::min(farStride, hashRefreshStrideFrames + ringDistance - 1);
      }
    }
    return std::max(1, stride);
  };

  HashSet<Vec2I> visibleChunkIndices;
  List<Vec2I> visibleChunkOrder;
  int chunkCountX = std::max(0, chunkRange.xMax() - chunkRange.xMin());
  int chunkCountY = std::max(0, chunkRange.yMax() - chunkRange.yMin());
  visibleChunkOrder.reserve((size_t)(chunkCountX * chunkCountY));

  for (int x = chunkRange.xMin(); x < chunkRange.xMax(); ++x) {
    for (int y = chunkRange.yMin(); y < chunkRange.yMax(); ++y) {
      auto chunkIndex = Vec2I{x, y};
      visibleChunkIndices.add(chunkIndex);
      visibleChunkOrder.append(chunkIndex);
    }
  }

  if (m_enableVisibleChunkPriority) {
    visibleChunkOrder.sort([&](Vec2I const& a, Vec2I const& b) {
      auto metricA = chunkDistanceMetric(a);
      auto metricB = chunkDistanceMetric(b);
      if (metricA != metricB)
        return metricA < metricB;
      if (a[0] != b[0])
        return a[0] < b[0];
      return a[1] < b[1];
    });
  }

  int criticalTerrainBuildsRemaining = worldGenerationChanged ? std::numeric_limits<int>::max() : std::max(0, m_criticalChunkSyncBuildsPerFrame);
  int criticalLiquidBuildsRemaining = criticalTerrainBuildsRemaining;

  for (auto const& chunkIndex : visibleChunkOrder) {
    bool hashRefreshed = false;
    bool allowHashRefresh = worldGenerationChanged
        || !m_enableChunkHashRefreshBudget
        || hashRefreshBudgetRemaining > 0
        || !m_cachedChunkHashes.contains(chunkIndex);
    auto [terrainHash, liquidHash] = cachedChunkHashes(
        renderData,
        chunkIndex,
        localHashStride(chunkIndex),
        overload,
        allowHashRefresh,
        &hashRefreshed);
    if (hashRefreshed && !worldGenerationChanged && m_enableChunkHashRefreshBudget && hashRefreshBudgetRemaining > 0)
      --hashRefreshBudgetRemaining;
    auto terrainChunk = getTerrainChunk(renderData, chunkIndex, terrainHash, terrainBudgetMicros);
    auto liquidChunk = getLiquidChunk(renderData, chunkIndex, liquidHash, liquidBudgetMicros);

    if (!terrainChunk) {
      if (auto staleTerrainChunk = m_lastTerrainChunks.ptr(chunkIndex)) {
        terrainChunk = *staleTerrainChunk;
      } else if (criticalTerrainBuildsRemaining > 0) {
        int64_t emergencyBudget = std::numeric_limits<int64_t>::max();
        terrainChunk = getTerrainChunk(renderData, chunkIndex, terrainHash, emergencyBudget);
        if (terrainChunk)
          --criticalTerrainBuildsRemaining;
      }
    }

    if (!liquidChunk) {
      if (auto staleLiquidChunk = m_lastLiquidChunks.ptr(chunkIndex)) {
        liquidChunk = *staleLiquidChunk;
      } else if (criticalLiquidBuildsRemaining > 0) {
        int64_t emergencyBudget = std::numeric_limits<int64_t>::max();
        liquidChunk = getLiquidChunk(renderData, chunkIndex, liquidHash, emergencyBudget);
        if (liquidChunk)
          --criticalLiquidBuildsRemaining;
      }
    }

    if (terrainChunk)
      m_lastTerrainChunks.set(chunkIndex, terrainChunk);
    if (liquidChunk)
      m_lastLiquidChunks.set(chunkIndex, liquidChunk);

    if (terrainChunk) {
      if (auto backgroundLayer = terrainChunk->ptr(TerrainLayer::Background)) {
        for (auto const& pair : *backgroundLayer)
          backgroundBuffersByZ[pair.first].append(pair.second);
      }

      if (auto midgroundLayer = terrainChunk->ptr(TerrainLayer::Midground)) {
        for (auto const& pair : *midgroundLayer)
          midgroundBuffersByZ[pair.first].append(pair.second);
      }

      if (auto foregroundLayer = terrainChunk->ptr(TerrainLayer::Foreground)) {
        for (auto const& pair : *foregroundLayer)
          foregroundBuffersByZ[pair.first].append(pair.second);
      }
    }

    if (liquidChunk) {
      for (auto const& pair : *liquidChunk)
        m_liquidBuffers.append(pair.second);
    }
  }

  int prefetchPerFrame = m_chunkPrefetchPerFrame;
  if (m_enableAdaptiveChunkPrefetchCadence && prefetchPerFrame > 0) {
    double prefetchScale = 1.0 - (overload * 0.8);
    int scaledPrefetch = (int)std::lround((double)prefetchPerFrame * prefetchScale);
    prefetchPerFrame = std::max(m_chunkPrefetchMinPerFrame, scaledPrefetch);
  }

  if (m_enableChunkPrefetch && m_chunkPrefetchRing > 0 && prefetchPerFrame > 0
      && (terrainBudgetMicros > 0 || liquidBudgetMicros > 0)) {
    RectI prefetchRange = chunkRange.padded(m_chunkPrefetchRing);
    List<Vec2I> prefetchOrder;

    for (int x = prefetchRange.xMin(); x < prefetchRange.xMax(); ++x) {
      for (int y = prefetchRange.yMin(); y < prefetchRange.yMax(); ++y) {
        Vec2I chunkIndex{x, y};
        if (!visibleChunkIndices.contains(chunkIndex))
          prefetchOrder.append(chunkIndex);
      }
    }

    if (m_enableVisibleChunkPriority) {
      prefetchOrder.sort([&](Vec2I const& a, Vec2I const& b) {
        auto metricA = chunkDistanceMetric(a);
        auto metricB = chunkDistanceMetric(b);
        if (metricA != metricB)
          return metricA < metricB;
        if (a[0] != b[0])
          return a[0] < b[0];
        return a[1] < b[1];
      });
    }

    int prefetched = 0;
    for (auto const& chunkIndex : prefetchOrder) {
      if (prefetched >= prefetchPerFrame)
        break;
      if (terrainBudgetMicros <= 0 && liquidBudgetMicros <= 0)
        break;

      bool hashRefreshed = false;
      bool allowHashRefresh = worldGenerationChanged
          || !m_enableChunkHashRefreshBudget
          || hashRefreshBudgetRemaining > 0
          || !m_cachedChunkHashes.contains(chunkIndex);
      auto [terrainHash, liquidHash] = cachedChunkHashes(
          renderData,
          chunkIndex,
          localHashStride(chunkIndex),
          overload,
          allowHashRefresh,
          &hashRefreshed);
      if (hashRefreshed && !worldGenerationChanged && m_enableChunkHashRefreshBudget && hashRefreshBudgetRemaining > 0)
        --hashRefreshBudgetRemaining;
      if (terrainBudgetMicros > 0)
        getTerrainChunk(renderData, chunkIndex, terrainHash, terrainBudgetMicros);
      if (liquidBudgetMicros > 0)
        getLiquidChunk(renderData, chunkIndex, liquidHash, liquidBudgetMicros);
      ++prefetched;
    }
  }

  for (auto const& chunkIndex : m_lastTerrainChunks.keys()) {
    if (!visibleChunkIndices.contains(chunkIndex))
      m_lastTerrainChunks.remove(chunkIndex);
  }

  for (auto const& chunkIndex : m_lastLiquidChunks.keys()) {
    if (!visibleChunkIndices.contains(chunkIndex))
      m_lastLiquidChunks.remove(chunkIndex);
  }

  RectI hashRetentionRange = chunkRange;
  if (m_enableChunkPrefetch && m_chunkPrefetchRing > 0)
    hashRetentionRange = hashRetentionRange.padded(m_chunkPrefetchRing);

  for (auto const& chunkIndex : m_cachedChunkHashes.keys()) {
    if (!hashRetentionRange.contains(chunkIndex))
      m_cachedChunkHashes.remove(chunkIndex);
  }

  for (auto& pair : backgroundBuffersByZ)
    m_backgroundTerrainBuffers.appendAll(std::move(pair.second));
  for (auto& pair : midgroundBuffersByZ)
    m_midgroundTerrainBuffers.appendAll(std::move(pair.second));
  for (auto& pair : foregroundBuffersByZ)
    m_foregroundTerrainBuffers.appendAll(std::move(pair.second));

  double setupCostMs = (Time::monotonicTime() - setupStart) * 1000.0;
  if (m_setupCostEmaMs <= 0.0)
    m_setupCostEmaMs = setupCostMs;
  else
    m_setupCostEmaMs += (setupCostMs - m_setupCostEmaMs) * m_setupCostSmoothing;
}

void TilePainter::renderBackground(WorldCamera const& camera) {
  renderTerrainChunks(camera, TerrainLayer::Background);
}

void TilePainter::renderMidground(WorldCamera const& camera) {
  renderTerrainChunks(camera, TerrainLayer::Midground);
}

void TilePainter::renderLiquid(WorldCamera const& /*camera*/) {
  for (auto const& buffer : m_liquidBuffers)
    m_renderer->renderBuffer(buffer, m_cameraTransformation);
}

void TilePainter::renderForeground(WorldCamera const& camera) {
  renderTerrainChunks(camera, TerrainLayer::Foreground);
}

void TilePainter::cleanup() {
  m_backgroundTerrainBuffers.clear();
  m_midgroundTerrainBuffers.clear();
  m_foregroundTerrainBuffers.clear();
  m_liquidBuffers.clear();
}

void TilePainter::cleanupCache() {
  if (m_reloadTracker->pullTriggered()) {
    refreshRenderConfig();
    m_cachedChunkHashes.clear();
    m_lastTerrainChunks.clear();
    m_lastLiquidChunks.clear();
    m_terrainChunkCache.clear();
    m_liquidChunkCache.clear();
  }

  m_textureCache.cleanup();
  m_terrainChunkCache.cleanup();
  m_liquidChunkCache.cleanup();
}

void TilePainter::refreshRenderConfig() {
  auto assets = Root::singleton().assets();
  auto renderingConfig = assets->json("/rendering.config");

  float cacheRetentionMultiplier = renderingConfig.optFloat("cacheRetentionMultiplier").value(1.0f);
  if (cacheRetentionMultiplier < 1.0f)
    cacheRetentionMultiplier = 1.0f;

  int64_t chunkCacheTimeout = (int64_t)(renderingConfig.getInt("chunkCacheTimeout") * cacheRetentionMultiplier);
  if (chunkCacheTimeout < 1)
    chunkCacheTimeout = 1;

  int64_t textureTimeout = (int64_t)(renderingConfig.getInt("textureTimeout") * cacheRetentionMultiplier);
  if (textureTimeout < 1)
    textureTimeout = 1;

  m_terrainChunkCache.setTimeToLive(chunkCacheTimeout);
  m_terrainChunkCache.setTimeSmear(chunkCacheTimeout / 4);

  m_liquidChunkCache.setTimeToLive(chunkCacheTimeout);
  m_liquidChunkCache.setTimeSmear(chunkCacheTimeout / 4);

  m_textureCache.setTimeToLive(textureTimeout);

  if (auto terrainChunkCacheMax = renderingConfig.optUInt("terrainChunkCacheMax"))
    m_terrainChunkCache.setMaxSize(*terrainChunkCacheMax);
  else
    m_terrainChunkCache.setMaxSize(NPos);

  if (auto liquidChunkCacheMax = renderingConfig.optUInt("liquidChunkCacheMax"))
    m_liquidChunkCache.setMaxSize(*liquidChunkCacheMax);
  else
    m_liquidChunkCache.setMaxSize(NPos);

  if (auto tileTextureCacheMax = renderingConfig.optUInt("tileTextureCacheMax"))
    m_textureCache.setMaxSize(*tileTextureCacheMax);
  else
    m_textureCache.setMaxSize(NPos);

  m_terrainDamageHashLevelQuantization = (uint8_t)std::clamp<uint64_t>(
      renderingConfig.optUInt("terrainDamageHashLevelQuantization").value(16), 1, 255);
  m_liquidHashLevelQuantization = (uint8_t)std::clamp<uint64_t>(
      renderingConfig.optUInt("liquidHashLevelQuantization").value(32), 1, 255);

  m_enableAdaptiveChunkBuildBudget = renderingConfig.optBool("adaptiveChunkBuildBudgetEnabled").value(true);

  m_baseTerrainChunkBuildBudgetMicros = (int64_t)std::clamp<uint64_t>(
      renderingConfig.optUInt("terrainChunkBuildBudgetMicros").value(3000), 64, 500000);
  m_baseLiquidChunkBuildBudgetMicros = (int64_t)std::clamp<uint64_t>(
      renderingConfig.optUInt("liquidChunkBuildBudgetMicros").value(1500), 64, 500000);
  m_minTerrainChunkBuildBudgetMicros = (int64_t)std::clamp<uint64_t>(
      renderingConfig.optUInt("terrainChunkBuildBudgetMinMicros").value(500), 16, 500000);
  m_minLiquidChunkBuildBudgetMicros = (int64_t)std::clamp<uint64_t>(
      renderingConfig.optUInt("liquidChunkBuildBudgetMinMicros").value(250), 16, 500000);

  if (m_minTerrainChunkBuildBudgetMicros > m_baseTerrainChunkBuildBudgetMicros)
    m_minTerrainChunkBuildBudgetMicros = m_baseTerrainChunkBuildBudgetMicros;
  if (m_minLiquidChunkBuildBudgetMicros > m_baseLiquidChunkBuildBudgetMicros)
    m_minLiquidChunkBuildBudgetMicros = m_baseLiquidChunkBuildBudgetMicros;

  m_chunkBuildBudgetOverloadStartMs = std::max(0.1, (double)renderingConfig.optFloat("adaptiveChunkBuildBudgetOverloadStartMs").value(6.0f));
  m_chunkBuildBudgetOverloadMaxMs = std::max(
      m_chunkBuildBudgetOverloadStartMs + 0.1,
      (double)renderingConfig.optFloat("adaptiveChunkBuildBudgetOverloadMaxMs").value(18.0f));
  m_setupCostSmoothing = std::clamp((double)renderingConfig.optFloat("adaptiveChunkBuildBudgetSmoothing").value(0.15f), 0.01, 1.0);

  m_enableAdaptiveChunkHashCadence = renderingConfig.optBool("adaptiveChunkHashCadenceEnabled").value(true);
  m_chunkHashRefreshBaseStrideFrames = (int)std::clamp<uint64_t>(
      renderingConfig.optUInt("adaptiveChunkHashCadenceBaseStrideFrames").value(1), 1, 32);
  m_chunkHashRefreshMaxStrideFrames = (int)std::clamp<uint64_t>(
      renderingConfig.optUInt("adaptiveChunkHashCadenceMaxStrideFrames").value(4), 1, 64);
  if (m_chunkHashRefreshMaxStrideFrames < m_chunkHashRefreshBaseStrideFrames)
    m_chunkHashRefreshMaxStrideFrames = m_chunkHashRefreshBaseStrideFrames;

  m_enableDistanceWeightedChunkHashCadence = renderingConfig.optBool("distanceWeightedChunkHashCadenceEnabled").value(true);
  m_chunkHashFarStrideFrames = (int)std::clamp<uint64_t>(
      renderingConfig.optUInt("distanceWeightedChunkHashCadenceFarStrideFrames").value(8), 1, 128);
  if (m_chunkHashFarStrideFrames < m_chunkHashRefreshBaseStrideFrames)
    m_chunkHashFarStrideFrames = m_chunkHashRefreshBaseStrideFrames;
  m_enableChunkHashRefreshBudget = renderingConfig.optBool("chunkHashRefreshBudgetEnabled").value(true);
  m_chunkHashRefreshBudgetPerFrame = (int)std::clamp<uint64_t>(
      renderingConfig.optUInt("chunkHashRefreshBudgetPerFrame").value(48), 1, 4096);
  m_enableAdaptiveChunkHashDebounce = renderingConfig.optBool("adaptiveChunkHashDebounceEnabled").value(true);
  m_chunkHashDebounceMismatchFrames = (int)std::clamp<uint64_t>(
      renderingConfig.optUInt("adaptiveChunkHashDebounceMismatchFrames").value(2), 1, 32);
  m_chunkHashDebounceOverloadThreshold = std::clamp(
      (double)renderingConfig.optFloat("adaptiveChunkHashDebounceOverloadThreshold").value(0.35f),
      0.0,
      1.0);

  m_enableVisibleChunkPriority = renderingConfig.optBool("visibleChunkPriorityEnabled").value(true);
  m_criticalChunkSyncBuildsPerFrame = (int)std::clamp<uint64_t>(
      renderingConfig.optUInt("criticalChunkSyncBuildsPerFrame").value(2), 0, 128);
  m_enableChunkPrefetch = renderingConfig.optBool("chunkPrefetchEnabled").value(true);
  m_enableAdaptiveChunkPrefetchCadence = renderingConfig.optBool("chunkPrefetchAdaptiveCadenceEnabled").value(true);
  m_chunkPrefetchRing = (int)std::clamp<uint64_t>(
      renderingConfig.optUInt("chunkPrefetchRing").value(1), 0, 8);
  m_chunkPrefetchPerFrame = (int)std::clamp<uint64_t>(
      renderingConfig.optUInt("chunkPrefetchPerFrame").value(16), 0, 256);
  m_chunkPrefetchMinPerFrame = (int)std::clamp<uint64_t>(
      renderingConfig.optUInt("chunkPrefetchMinPerFrame").value(2), 0, 256);
  if (m_chunkPrefetchMinPerFrame > m_chunkPrefetchPerFrame)
    m_chunkPrefetchMinPerFrame = m_chunkPrefetchPerFrame;
}

size_t TilePainter::TextureKeyHash::operator()(TextureKey const& key) const {
  if (key.is<MaterialPieceTextureKey>())
    return hashOf(key.typeIndex(), key.get<MaterialPieceTextureKey>());
  else
    return hashOf(key.typeIndex(), key.get<AssetTextureKey>());
}

pair<TilePainter::ChunkHash, TilePainter::ChunkHash> TilePainter::chunkHashes(WorldRenderData& renderData, Vec2I chunkIndex) const {
  XXHash3 terrainHasher;
  XXHash3 liquidHasher;
  RectI tileRange = RectI::withSize(chunkIndex * RenderChunkSize, Vec2I::filled(RenderChunkSize)).padded(MaterialRenderProfileMaxNeighborDistance);

  forEachRenderTile(renderData, tileRange, [&](Vec2I const&, RenderTile const& renderTile) {
    if (m_terrainDamageHashLevelQuantization <= 1) {
      renderTile.hashPushTerrain(terrainHasher);
    } else {
      RenderTile quantizedTile = renderTile;
      quantizedTile.foregroundDamageLevel = (uint8_t)((quantizedTile.foregroundDamageLevel / m_terrainDamageHashLevelQuantization) * m_terrainDamageHashLevelQuantization);
      quantizedTile.backgroundDamageLevel = (uint8_t)((quantizedTile.backgroundDamageLevel / m_terrainDamageHashLevelQuantization) * m_terrainDamageHashLevelQuantization);
      quantizedTile.hashPushTerrain(terrainHasher);
    }

    uint8_t liquidLevel = renderTile.liquidLevel;
    if (m_liquidHashLevelQuantization > 1) {
      liquidLevel = (uint8_t)((liquidLevel / m_liquidHashLevelQuantization) * m_liquidHashLevelQuantization);
    }

    xxHash3Push(liquidHasher, (unsigned int)renderTile.liquidId);
    xxHash3Push(liquidHasher, (unsigned int)liquidLevel);
  });

  return {terrainHasher.digest(), liquidHasher.digest()};
}

pair<TilePainter::ChunkHash, TilePainter::ChunkHash> TilePainter::cachedChunkHashes(
    WorldRenderData& renderData,
    Vec2I chunkIndex,
    int hashRefreshStrideFrames,
    double overload,
    bool allowRefresh,
    bool* refreshed) {
  if (refreshed)
    *refreshed = false;

  auto commitHashesWithDebounce = [&](CachedChunkHashes* cached, pair<ChunkHash, ChunkHash> const& hashes) {
    bool changed = false;

    auto commitOne = [&](ChunkHash newHash, ChunkHash& cachedHash, uint16_t& mismatchStreak) {
      if (newHash == cachedHash) {
        mismatchStreak = 0;
        return cachedHash;
      }

      uint16_t requiredMismatches = (uint16_t)std::max(1, m_chunkHashDebounceMismatchFrames);
      bool debounceActive = m_enableAdaptiveChunkHashDebounce
          && overload >= m_chunkHashDebounceOverloadThreshold
          && requiredMismatches > 1;
      if (debounceActive && mismatchStreak + 1 < requiredMismatches) {
        ++mismatchStreak;
        return cachedHash;
      }

      cachedHash = newHash;
      mismatchStreak = 0;
      changed = true;
      return cachedHash;
    };

    ChunkHash terrainHash = commitOne(hashes.first, cached->terrainHash, cached->terrainMismatchStreak);
    ChunkHash liquidHash = commitOne(hashes.second, cached->liquidHash, cached->liquidMismatchStreak);
    if (changed && refreshed)
      *refreshed = true;
    return std::make_pair(terrainHash, liquidHash);
  };

  if (auto cached = m_cachedChunkHashes.ptr(chunkIndex)) {
    if (!allowRefresh)
      return {cached->terrainHash, cached->liquidHash};

    if (hashRefreshStrideFrames > 1) {
      uint64_t phase = hashOf(chunkIndex[0], chunkIndex[1]) % (uint64_t)hashRefreshStrideFrames;
      uint64_t framePhase = m_setupFrameIndex % (uint64_t)hashRefreshStrideFrames;
      if (phase != framePhase)
        return {cached->terrainHash, cached->liquidHash};
    }

    auto hashes = chunkHashes(renderData, chunkIndex);
    return commitHashesWithDebounce(cached, hashes);
  }

  auto hashes = chunkHashes(renderData, chunkIndex);
  m_cachedChunkHashes.set(chunkIndex, CachedChunkHashes{hashes.first, hashes.second, 0, 0});
  if (refreshed)
    *refreshed = true;
  return hashes;
}

void TilePainter::renderTerrainChunks(WorldCamera const& /*camera*/, TerrainLayer terrainLayer) {
  List<RenderBufferPtr> const* terrainBuffers = nullptr;
  if (terrainLayer == TerrainLayer::Background)
    terrainBuffers = &m_backgroundTerrainBuffers;
  else if (terrainLayer == TerrainLayer::Midground)
    terrainBuffers = &m_midgroundTerrainBuffers;
  else
    terrainBuffers = &m_foregroundTerrainBuffers;

  for (auto const& buffer : *terrainBuffers)
    m_renderer->renderBuffer(buffer, m_cameraTransformation);

  m_renderer->flush();
}

shared_ptr<TilePainter::TerrainChunk const> TilePainter::buildTerrainChunk(WorldRenderData& renderData, Vec2I chunkIndex) {
  HashMap<TerrainLayer, HashMap<QuadZLevel, List<RenderPrimitive>>> terrainPrimitives;

  RectI tileRange = RectI::withSize(chunkIndex * RenderChunkSize, Vec2I::filled(RenderChunkSize));
  for (int x = tileRange.xMin(); x < tileRange.xMax(); ++x) {
    for (int y = tileRange.yMin(); y < tileRange.yMax(); ++y) {
      bool occluded = this->produceTerrainPrimitives(terrainPrimitives[TerrainLayer::Foreground], TerrainLayer::Foreground, {x, y}, renderData);
      occluded = this->produceTerrainPrimitives(terrainPrimitives[TerrainLayer::Midground], TerrainLayer::Midground, {x, y}, renderData) || occluded;
      if (!occluded)
        this->produceTerrainPrimitives(terrainPrimitives[TerrainLayer::Background], TerrainLayer::Background, {x, y}, renderData);
    }
  }

  auto chunk = make_shared<TerrainChunk>();
  for (auto& layerPair : terrainPrimitives) {
    for (auto& zLevelPair : layerPair.second) {
      auto rb = m_renderer->createRenderBuffer();
      rb->set(zLevelPair.second);
      (*chunk)[layerPair.first][zLevelPair.first] = std::move(rb);
    }
  }

  return chunk;
}

shared_ptr<TilePainter::TerrainChunk const> TilePainter::getTerrainChunk(WorldRenderData& renderData, Vec2I chunkIndex, ChunkHash terrainHash, int64_t& terrainBudgetMicros) {
  pair<Vec2I, ChunkHash> chunkKey = {chunkIndex, terrainHash};
  if (auto cachedChunk = m_terrainChunkCache.ptr(chunkKey))
    return *cachedChunk;

  if (terrainBudgetMicros <= 0)
    return {};

  double buildStart = Time::monotonicTime();
  auto chunk = buildTerrainChunk(renderData, chunkIndex);
  int64_t buildMicros = std::max<int64_t>(1, (int64_t)((Time::monotonicTime() - buildStart) * 1000000.0));
  terrainBudgetMicros -= buildMicros;
  m_terrainChunkCache.set(chunkKey, chunk);
  return chunk;
}

shared_ptr<TilePainter::LiquidChunk const> TilePainter::buildLiquidChunk(WorldRenderData& renderData, Vec2I chunkIndex) {
  HashMap<LiquidId, List<RenderPrimitive>> liquidPrimitives;

  RectI tileRange = RectI::withSize(chunkIndex * RenderChunkSize, Vec2I::filled(RenderChunkSize));
  for (int x = tileRange.xMin(); x < tileRange.xMax(); ++x) {
    for (int y = tileRange.yMin(); y < tileRange.yMax(); ++y)
      this->produceLiquidPrimitives(liquidPrimitives, {x, y}, renderData);
  }

  auto chunk = make_shared<LiquidChunk>();
  for (auto& p : liquidPrimitives) {
    auto rb = m_renderer->createRenderBuffer();
    rb->set(p.second);
    chunk->set(p.first, std::move(rb));
  }

  return chunk;
}

shared_ptr<TilePainter::LiquidChunk const> TilePainter::getLiquidChunk(WorldRenderData& renderData, Vec2I chunkIndex, ChunkHash liquidHash, int64_t& liquidBudgetMicros) {
  pair<Vec2I, ChunkHash> chunkKey = {chunkIndex, liquidHash};
  if (auto cachedChunk = m_liquidChunkCache.ptr(chunkKey))
    return *cachedChunk;

  if (liquidBudgetMicros <= 0)
    return {};

  double buildStart = Time::monotonicTime();
  auto chunk = buildLiquidChunk(renderData, chunkIndex);
  int64_t buildMicros = std::max<int64_t>(1, (int64_t)((Time::monotonicTime() - buildStart) * 1000000.0));
  liquidBudgetMicros -= buildMicros;
  m_liquidChunkCache.set(chunkKey, chunk);
  return chunk;
}

bool TilePainter::produceTerrainPrimitives(HashMap<QuadZLevel, List<RenderPrimitive>>& primitives,
    TerrainLayer terrainLayer, Vec2I const& pos, WorldRenderData const& renderData) {
  auto& root = Root::singleton();
  auto assets = Root::singleton().assets();
  auto materialDatabase = root.materialDatabase();

  RenderTile const& tile = getRenderTile(renderData, pos);

  MaterialId material = EmptyMaterialId;
  MaterialHue materialHue = 0;
  MaterialColorVariant colorVariant = 0;
  ModId mod = NoModId;
  MaterialHue modHue = 0;
  float damageLevel = 0.0f;
  TileDamageType damageType = TileDamageType::Protected;
  Vec4B color;

  bool occlude = false;

  if (terrainLayer == TerrainLayer::Background) {
    material = tile.background;
    materialHue = tile.backgroundHueShift;
    colorVariant = tile.backgroundColorVariant;
    mod = tile.backgroundMod;
    modHue = tile.backgroundModHueShift;
    damageLevel = byteToFloat(tile.backgroundDamageLevel);
    damageType = tile.backgroundDamageType;
    color = m_backgroundLayerColor;
  } else {
    material = tile.foreground;
    materialHue = tile.foregroundHueShift;
    colorVariant = tile.foregroundColorVariant;
    mod = tile.foregroundMod;
    modHue = tile.foregroundModHueShift;
    damageLevel = byteToFloat(tile.foregroundDamageLevel);
    damageType = tile.foregroundDamageType;
    color = m_foregroundLayerColor;
  }

  // render non-block colliding things in the midground
  bool isBlock = BlockCollisionSet.contains(materialDatabase->materialCollisionKind(material));
  if (terrainLayer == (isBlock ? TerrainLayer::Midground : TerrainLayer::Foreground))
    return false;

  auto getPieceTexture = [this, assets](MaterialId material, MaterialRenderPieceConstPtr const& piece, MaterialHue hue, Directives const* directives, bool mod) {
    return m_textureCache.get(MaterialPieceTextureKey(material, piece->pieceId, hue, mod), [&](auto const&) {
        AssetPath texture = (hue == 0) ? piece->texture : strf("{}?hueshift={}", piece->texture, materialHueToDegrees(hue));

        if (directives)
          texture.directives += *directives;

        return m_textureGroup->create(*assets->image(texture));
      });
  };

  auto materialRenderProfile = materialDatabase->materialRenderProfile(material);
  auto modRenderProfile = materialDatabase->modRenderProfile(mod);

  if (materialRenderProfile) {
    occlude = materialRenderProfile->occludesBehind;
    auto materialColorVariant = materialRenderProfile->colorVariants > 0 ? colorVariant % materialRenderProfile->colorVariants : 0;
    uint32_t variance = staticRandomU32(renderData.geometry.xwrap(pos[0]), pos[1], (int)terrainLayer, "main");
    auto& quadList = primitives[materialZLevel(materialRenderProfile->zLevel, material, materialHue, materialColorVariant)];

    MaterialPieceResultList pieces;
    determineMatchingPieces(pieces, &occlude, materialDatabase, materialRenderProfile->mainMatchList, renderData, pos,
        terrainLayer == TerrainLayer::Background ? TileLayer::Background : TileLayer::Foreground, false);
    Directives const* directives = materialRenderProfile->colorDirectives.empty()
      ? nullptr
      : &materialRenderProfile->colorDirectives.wrap(materialColorVariant);
    for (auto const& piecePair : pieces) {
      TexturePtr texture = getPieceTexture(material, piecePair.first, materialHue, directives, false);
      auto variant = piecePair.first->variants.ptr(materialColorVariant);
      if (!variant) variant = piecePair.first->variants.ptr(0);
      if (!variant) continue;
      RectF textureCoords = variant->wrap(variance);
      RectF worldCoords = RectF::withSize(piecePair.second / TilePixels + Vec2F(pos), textureCoords.size() / TilePixels);
      quadList.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(texture),
          worldCoords  .min(),
          textureCoords.min(),
          Vec2F(  worldCoords.xMax(),   worldCoords.yMin()),
          Vec2F(textureCoords.xMax(), textureCoords.yMin()),
          worldCoords  .max(),
          textureCoords.max(),
          Vec2F(  worldCoords.xMin(),   worldCoords.yMax()),
          Vec2F(textureCoords.xMin(), textureCoords.yMax()),
          color, 1.0f);
    }
  }

  if (modRenderProfile) {
    auto modColorVariant = modRenderProfile->colorVariants > 0 ? colorVariant % modRenderProfile->colorVariants : 0;
    uint32_t variance = staticRandomU32(renderData.geometry.xwrap(pos[0]), pos[1], (int)terrainLayer, "mod");
    auto& quadList = primitives[modZLevel(modRenderProfile->zLevel, mod, modHue, modColorVariant)];

    MaterialPieceResultList pieces;
    determineMatchingPieces(pieces, &occlude, materialDatabase, modRenderProfile->mainMatchList, renderData, pos,
        terrainLayer == TerrainLayer::Background ? TileLayer::Background : TileLayer::Foreground, true);
    Directives const* directives = modRenderProfile->colorDirectives.empty()
      ? nullptr
      : &modRenderProfile->colorDirectives.wrap(modColorVariant);
    for (auto const& piecePair : pieces) {
      auto texture = getPieceTexture(mod, piecePair.first, modHue, directives, true);
      auto variant = piecePair.first->variants.ptr(modColorVariant);
      if (!variant) variant = piecePair.first->variants.ptr(0);
      if (!variant) continue;
      auto& textureCoords = variant->wrap(variance);
      RectF worldCoords = RectF::withSize(piecePair.second / TilePixels + Vec2F(pos), textureCoords.size() / TilePixels);
      quadList.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(texture),
          worldCoords.min(), textureCoords.min(),
          Vec2F(worldCoords.xMax(), worldCoords.yMin()), Vec2F(textureCoords.xMax(), textureCoords.yMin()),
          worldCoords.max(), textureCoords.max(),
          Vec2F(worldCoords.xMin(), worldCoords.yMax()), Vec2F(textureCoords.xMin(), textureCoords.yMax()),
        color, 1.0f);
    }
  }

  if (materialRenderProfile && damageLevel > 0 && isBlock) {
    auto& quadList = primitives[damageZLevel()];
    auto const& crackingImage = materialRenderProfile->damageImage(damageLevel, damageType);

    TexturePtr texture = m_textureCache.get(AssetTextureKey(crackingImage.first),
        [&](auto const&) { return m_textureGroup->create(*assets->image(crackingImage.first)); });

    Vec2F textureSize(texture->size());
    RectF textureCoords = RectF::withSize(Vec2F(), textureSize);
    RectF worldCoords = RectF::withSize(crackingImage.second / TilePixels + Vec2F(pos), textureCoords.size() / TilePixels);

    quadList.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(texture),
        worldCoords.min(), textureCoords.min(),
        Vec2F(worldCoords.xMax(), worldCoords.yMin()), Vec2F(textureCoords.xMax(), textureCoords.yMin()),
        worldCoords.max(), textureCoords.max(),
        Vec2F(worldCoords.xMin(), worldCoords.yMax()), Vec2F(textureCoords.xMin(), textureCoords.yMax()),
      color, 1.0f);
  }

  return occlude;
}

void TilePainter::produceLiquidPrimitives(HashMap<LiquidId, List<RenderPrimitive>>& primitives, Vec2I const& pos, WorldRenderData const& renderData) {
  RenderTile const& tile = getRenderTile(renderData, pos);

  float drawLevel = liquidDrawLevel(byteToFloat(tile.liquidLevel));
  if (drawLevel <= 0.0f)
    return;

  RenderTile const& tileBottom = getRenderTile(renderData, pos - Vec2I(0, 1));
  float bottomDrawLevel = liquidDrawLevel(byteToFloat(tileBottom.liquidLevel));

  RectF worldRect;
  if (tileBottom.foreground == EmptyMaterialId && bottomDrawLevel < 1.0f)
    worldRect = RectF::withSize(Vec2F(pos), Vec2F::filled(1.0f)).expanded(drawLevel);
  else
    worldRect = RectF::withSize(Vec2F(pos), Vec2F(1.0f, drawLevel));

  auto texRect = worldRect.scaled(TilePixels);

  auto const& liquid = m_liquids[tile.liquidId];
  primitives[tile.liquidId].emplace_back(std::in_place_type_t<RenderQuad>(), liquid.texture,
      worldRect.min(), texRect.min(),
      Vec2F(worldRect.xMax(), worldRect.yMin()), Vec2F(texRect.xMax(), texRect.yMin()),
      worldRect.max(), texRect.max(),
      Vec2F(worldRect.xMin(), worldRect.yMax()), Vec2F(texRect.xMin(), texRect.yMax()),
    liquid.color, 1.0f);
}

float TilePainter::liquidDrawLevel(float liquidLevel) const {
  return clamp((liquidLevel - m_liquidDrawLevels[0]) / (m_liquidDrawLevels[1] - m_liquidDrawLevels[0]), 0.0f, 1.0f);
}

}
