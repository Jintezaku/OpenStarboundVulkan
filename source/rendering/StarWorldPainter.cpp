#include "StarWorldPainter.hpp"
#include "StarAnimation.hpp"
#include "StarRoot.hpp"
#include "StarConfiguration.hpp"
#include "StarAssets.hpp"
#include "StarAlgorithm.hpp"
#include "StarJsonExtra.hpp"
#include "StarTime.hpp"
#include <cmath>

namespace Star {

WorldPainter::WorldPainter() {
  m_assets = Root::singleton().assets();
  m_reloadTracker = make_shared<TrackerListener>();
  Root::singleton().registerReloadListener(m_reloadTracker);
  m_nextCleanupTime = 0;
  m_cacheCleanupInterval = 1500;
  m_cacheCleanupPhase = 0;
  m_previousFrameRenderMs = 0.0f;
  m_frameCostEmaMs = 0.0f;
  m_frameCostEmaSmoothing = 0.18f;
  m_particleRenderCapPerLayer = 2048;
  m_particleRenderCapPerLayerMin = 256;
  m_particleAdaptiveBudgetFrameMs = 16.7f;
  m_particleRenderLayerBudgetMs = 3.5f;
  m_particleRenderLayerBudgetMinMs = 0.8f;
  m_drawableCullBypassThreshold = 2048;
  m_drawableCullBypassMaxEntityDrawables = 4096;
  m_drawableCullBypassWarmupFrames = 90;
  m_drawableCullBypassMaxConsecutiveFrames = 1;
  m_drawableCullBypassMaxFrameMs = 14.0f;
  m_drawableWorldCoarseCullPadding = 16.0f;
  m_skipDrawableCulling = false;
  m_offscreenPreloadCapPerFrame = 12;
  m_offscreenPreloadCount = 0;
  m_fastFrameStreak = 0;
  m_drawableCullBypassStreak = 0;

  m_camera.setScreenSize({800, 600});
  m_camera.setCenterWorldPosition(Vec2F());
  m_camera.setPixelRatio(Root::singleton().configuration()->get("zoomLevel").toFloat());

  m_highlightConfig = m_assets->json("/highlights.config");
  m_maxHighlightLevel = m_highlightConfig.getFloat("maxHighlightLevel", 1.0f);
  for (auto p : m_highlightConfig.get("highlightDirectives").iterateObject())
    m_highlightDirectives.set(EntityHighlightEffectTypeNames.getLeft(p.first), {p.second.getString("underlay", ""), p.second.getString("overlay", "")});

  m_entityBarOffset = jsonToVec2F(m_assets->json("/rendering.config:entityBarOffset"));
  m_entityBarSpacing = jsonToVec2F(m_assets->json("/rendering.config:entityBarSpacing"));
  m_entityBarSize = jsonToVec2F(m_assets->json("/rendering.config:entityBarSize"));
  m_entityBarIconOffset = jsonToVec2F(m_assets->json("/rendering.config:entityBarIconOffset"));
  refreshRenderConfig();
}

void WorldPainter::renderInit(RendererPtr renderer) {
  m_assets = Root::singleton().assets();

  m_renderer = std::move(renderer);
  auto textureGroup = m_renderer->createTextureGroup(TextureGroupSize::Large);
  m_textPainter = make_shared<TextPainter>(m_renderer, textureGroup);
  m_tilePainter = make_shared<TilePainter>(m_renderer);
  m_drawablePainter = make_shared<DrawablePainter>(m_renderer, make_shared<AssetTextureGroup>(textureGroup));
  m_environmentPainter = make_shared<EnvironmentPainter>(m_renderer);
}

void WorldPainter::setCameraPosition(WorldGeometry const& geometry, Vec2F const& position) {
  m_camera.setWorldGeometry(geometry);
  m_camera.setCenterWorldPosition(position);
}

WorldCamera& WorldPainter::camera() {
  return m_camera;
}

void WorldPainter::update(float dt) {
  m_environmentPainter->update(dt);
}

void WorldPainter::render(WorldRenderData& renderData, function<bool()> lightWaiter) {
  int64_t renderFrameStartUs = Time::monotonicMicroseconds();
  m_offscreenPreloadCount = 0;

  m_camera.setScreenSize(m_renderer->screenSize());
  m_camera.setTargetPixelRatio(Root::singleton().configuration()->get("zoomLevel").toFloat());
  m_worldScreenRect = RectF::withSize(Vec2F(), Vec2F(m_camera.screenSize()));
  m_worldCoarseCullRect = m_camera.worldScreenRect().padded(m_drawableWorldCoarseCullPadding);

  m_assets = Root::singleton().assets();
  if (m_reloadTracker->pullTriggered())
    refreshRenderConfig();

  m_tilePainter->setup(m_camera, renderData);

  m_backParticles.clear();
  m_middleParticles.clear();
  m_frontParticles.clear();
  if (renderData.particles) {
    m_backParticles.reserve(renderData.particles->size() / 3);
    m_middleParticles.reserve(renderData.particles->size() / 3);
    m_frontParticles.reserve(renderData.particles->size() / 3);
    for (auto const& particle : *renderData.particles) {
      if (particle.layer == Particle::Layer::Back)
        m_backParticles.append(&particle);
      else if (particle.layer == Particle::Layer::Middle)
        m_middleParticles.append(&particle);
      else if (particle.layer == Particle::Layer::Front)
        m_frontParticles.append(&particle);
    }
  }

  // Stars, Debris Fields, Sky, and Orbiters

  // Use a fixed pixel ratio for certain things.
  float pixelRatioBasis = m_camera.screenSize()[1] / 1080.0f;
  float starAndDebrisRatio = lerp(0.0625f, pixelRatioBasis * 2.0f, m_camera.pixelRatio());
  float orbiterAndPlanetRatio = lerp(0.125f, pixelRatioBasis * 3.0f, m_camera.pixelRatio());

  m_environmentPainter->renderStars(starAndDebrisRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData);
  m_environmentPainter->renderDebrisFields(starAndDebrisRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData);
  if (renderData.skyRenderData.type != SkyType::Atmosphereless)
    m_environmentPainter->renderBackOrbiters(orbiterAndPlanetRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData);
  m_environmentPainter->renderPlanetHorizon(orbiterAndPlanetRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData);
  m_environmentPainter->renderSky(Vec2F(m_camera.screenSize()), renderData.skyRenderData);
  m_environmentPainter->renderFrontOrbiters(orbiterAndPlanetRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData);
  if (renderData.skyRenderData.type == SkyType::Atmosphereless)
    m_environmentPainter->renderBackOrbiters(orbiterAndPlanetRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData);

  m_renderer->flush();

  bool lightMapUpdated = lightWaiter ? lightWaiter() : false;

  m_renderer->setEffectParameter("lightMapEnabled", !renderData.isFullbright);
  if (renderData.isFullbright) {
    m_renderer->setEffectTexture("lightMap", Image::filled(Vec2U(1, 1), { 255, 255, 255, 255 }, PixelFormat::RGB24));
    m_renderer->setEffectParameter("lightMapMultiplier", 1.0f);
  } else {
    if (lightMapUpdated) {
      adjustLighting(renderData);
      m_renderer->setEffectTexture("lightMap", renderData.lightMap);
    }
    m_renderer->setEffectParameter("lightMapMultiplier", m_lightMapMultiplier);
    m_renderer->setEffectParameter("lightMapScale", Vec2F::filled(TilePixels * m_camera.pixelRatio()));
    m_renderer->setEffectParameter("lightMapOffset", m_camera.worldToScreen(Vec2F(renderData.lightMinPosition)));
  }

  // Parallax layers

  auto parallaxDelta = m_camera.worldGeometry().diff(m_camera.centerWorldPosition(), m_previousCameraCenter);
  if (parallaxDelta.magnitude() > 10)
    m_parallaxWorldPosition = m_camera.centerWorldPosition();
  else
    m_parallaxWorldPosition += parallaxDelta;
  m_previousCameraCenter = m_camera.centerWorldPosition();
  m_parallaxWorldPosition[1] = m_camera.centerWorldPosition()[1];

  if (!renderData.parallaxLayers.empty())
    m_environmentPainter->renderParallaxLayers(m_parallaxWorldPosition, m_camera, renderData.parallaxLayers, renderData.skyRenderData);

  // Main world layers
  auto& entityDrawables = renderData.entityLayerDrawables;

  bool previousFrameFast = m_previousFrameRenderMs <= m_drawableCullBypassMaxFrameMs;
  if (previousFrameFast)
    ++m_fastFrameStreak;
  else
    m_fastFrameStreak = 0;

  bool cullBypassEligible = m_drawableCullBypassThreshold > 0
      && renderData.entityDrawableCount >= m_drawableCullBypassThreshold
      && renderData.entityDrawableCount <= m_drawableCullBypassMaxEntityDrawables
      && previousFrameFast
      && m_fastFrameStreak >= m_drawableCullBypassWarmupFrames
      && m_drawableCullBypassStreak < m_drawableCullBypassMaxConsecutiveFrames;

  m_skipDrawableCulling = cullBypassEligible;
  if (m_skipDrawableCulling)
    ++m_drawableCullBypassStreak;
  else
    m_drawableCullBypassStreak = 0;

  size_t entityDrawableIndex = 0;
  auto renderEntitiesUntil = [this, &renderData, &entityDrawables, &entityDrawableIndex](Maybe<EntityRenderLayer> until) {
    bool drewLayer = false;
    while (true) {
      if (entityDrawableIndex >= entityDrawables.size())
        break;
      if (until && entityDrawables[entityDrawableIndex].layer >= *until)
        break;

      auto& entityDrawable = entityDrawables[entityDrawableIndex];
      drawEntityLayer(renderData.entityDrawablesArena, entityDrawable.drawablesBegin, entityDrawable.drawablesCount, entityDrawable.highlightEffect);
      ++entityDrawableIndex;
      drewLayer = true;
    }

    if (drewLayer)
      m_renderer->flush();
  };

  renderEntitiesUntil(RenderLayerBackgroundOverlay);
  drawDrawableSet(renderData.backgroundOverlays);
  renderEntitiesUntil(RenderLayerBackgroundTile);
  m_tilePainter->renderBackground(m_camera);
  renderEntitiesUntil(RenderLayerPlatform);
  m_tilePainter->renderMidground(m_camera);
  renderEntitiesUntil(RenderLayerBackParticle);
  renderParticles(renderData, Particle::Layer::Back);
  renderEntitiesUntil(RenderLayerLiquid);
  m_tilePainter->renderLiquid(m_camera);
  renderEntitiesUntil(RenderLayerMiddleParticle);
  renderParticles(renderData, Particle::Layer::Middle);
  renderEntitiesUntil(RenderLayerForegroundTile);
  m_tilePainter->renderForeground(m_camera);
  renderEntitiesUntil(RenderLayerForegroundOverlay);
  drawDrawableSet(renderData.foregroundOverlays);
  renderEntitiesUntil(RenderLayerFrontParticle);
  renderParticles(renderData, Particle::Layer::Front);
  renderEntitiesUntil(RenderLayerOverlay);
  drawDrawableSet(renderData.nametags);
  renderBars(renderData);
  renderEntitiesUntil({});

  auto dimLevel = round(renderData.dimLevel * 255);
  if (dimLevel != 0)
    m_renderer->render(renderFlatRect(RectF::withSize({}, Vec2F(m_camera.screenSize())), Vec4B(renderData.dimColor, dimLevel), 0.0f));

  m_tilePainter->cleanup();

  // World light-mapping must not bleed into UI layers rendered after world.
  m_renderer->setEffectParameter("lightMapEnabled", false);
  m_renderer->setEffectParameter("lightMapMultiplier", 1.0f);

  int64_t now = Time::monotonicMilliseconds();
  if (now >= m_nextCleanupTime) {
    // Skip cleanup during overloaded frames to avoid amplifying stutter spikes.
    bool allowCleanup = m_previousFrameRenderMs <= 16.7f
        || (now - m_nextCleanupTime) >= (m_cacheCleanupInterval * 6);
    if (allowCleanup) {
      // Stagger cache cleanup to avoid periodic one-frame stalls in heavy scenes.
      if (m_cacheCleanupPhase == 0) {
        m_textPainter->cleanup(m_textureTimeout);
        m_drawablePainter->cleanup(m_textureTimeout);
      } else if (m_cacheCleanupPhase == 1) {
        m_environmentPainter->cleanup(m_textureTimeout);
      } else {
        m_tilePainter->cleanupCache();
      }

      m_cacheCleanupPhase = (uint8_t)((m_cacheCleanupPhase + 1) % 3);
      m_nextCleanupTime = now + m_cacheCleanupInterval;
    } else {
      m_nextCleanupTime = now + 100;
    }
  }

  m_previousFrameRenderMs = (float)(Time::monotonicMicroseconds() - renderFrameStartUs) / 1000.0f;
  if (m_frameCostEmaMs <= 0.0f)
    m_frameCostEmaMs = m_previousFrameRenderMs;
  else
    m_frameCostEmaMs += (m_previousFrameRenderMs - m_frameCostEmaMs) * m_frameCostEmaSmoothing;
}

void WorldPainter::adjustLighting(WorldRenderData& renderData) {
  m_tilePainter->adjustLighting(renderData);
}

void WorldPainter::refreshRenderConfig() {
  auto renderingConfig = m_assets->json("/rendering.config");

  float cacheRetentionMultiplier = renderingConfig.optFloat("cacheRetentionMultiplier").value(1.0f);
  if (cacheRetentionMultiplier < 1.0f)
    cacheRetentionMultiplier = 1.0f;

  m_preloadTextureChance = renderingConfig.getFloat("preloadTextureChance");
  m_lightMapMultiplier = renderingConfig.getFloat("lightMapMultiplier");
  m_textParticleFontSize = renderingConfig.getInt("textParticleFontSize");
  m_particleRenderWindowPadding = renderingConfig.getInt("particleRenderWindowPadding");
  m_particleRenderCapPerLayer = renderingConfig.optUInt("particleRenderCapPerLayer").value(2048);
  m_particleRenderCapPerLayerMin = renderingConfig.optUInt("particleRenderCapPerLayerMin").value(256);
  m_particleAdaptiveBudgetFrameMs = renderingConfig.optFloat("particleAdaptiveBudgetFrameMs").value(16.7f);
  m_particleRenderLayerBudgetMs = renderingConfig.optFloat("particleRenderLayerBudgetMs").value(3.5f);
  m_particleRenderLayerBudgetMinMs = renderingConfig.optFloat("particleRenderLayerBudgetMinMs").value(0.8f);
  if (m_particleRenderCapPerLayerMin > m_particleRenderCapPerLayer)
    m_particleRenderCapPerLayerMin = m_particleRenderCapPerLayer;
  if (m_particleAdaptiveBudgetFrameMs < 0.0f)
    m_particleAdaptiveBudgetFrameMs = 0.0f;
  if (m_particleRenderLayerBudgetMs < 0.0f)
    m_particleRenderLayerBudgetMs = 0.0f;
  if (m_particleRenderLayerBudgetMinMs < 0.0f)
    m_particleRenderLayerBudgetMinMs = 0.0f;
  if (m_particleRenderLayerBudgetMinMs > m_particleRenderLayerBudgetMs)
    m_particleRenderLayerBudgetMinMs = m_particleRenderLayerBudgetMs;
  m_frameCostEmaSmoothing = renderingConfig.optFloat("frameCostEmaSmoothing").value(0.18f);
  if (m_frameCostEmaSmoothing < 0.01f)
    m_frameCostEmaSmoothing = 0.01f;
  else if (m_frameCostEmaSmoothing > 1.0f)
    m_frameCostEmaSmoothing = 1.0f;
  m_drawableCullBypassThreshold = renderingConfig.optUInt("drawableCullBypassThreshold").value(2048);
  m_drawableCullBypassMaxEntityDrawables = renderingConfig.optUInt("drawableCullBypassMaxEntityDrawables").value(4096);
  if (m_drawableCullBypassMaxEntityDrawables < m_drawableCullBypassThreshold)
    m_drawableCullBypassMaxEntityDrawables = m_drawableCullBypassThreshold;
  m_drawableCullBypassWarmupFrames = renderingConfig.optUInt("drawableCullBypassWarmupFrames").value(90);
  m_drawableCullBypassMaxConsecutiveFrames = renderingConfig.optUInt("drawableCullBypassMaxConsecutiveFrames").value(1);
  m_drawableCullBypassMaxFrameMs = renderingConfig.optFloat("drawableCullBypassMaxFrameMs").value(14.0f);
  if (m_drawableCullBypassMaxFrameMs < 0.0f)
    m_drawableCullBypassMaxFrameMs = 0.0f;
  m_offscreenPreloadCapPerFrame = renderingConfig.optUInt("offscreenPreloadCapPerFrame").value(12);
  m_drawableWorldCoarseCullPadding = renderingConfig.optFloat("drawableWorldCoarseCullPadding").value(16.0f);
  if (m_drawableWorldCoarseCullPadding < 0.0f)
    m_drawableWorldCoarseCullPadding = 0.0f;

  m_textureTimeout = (int64_t)(renderingConfig.getInt("textureTimeout") * cacheRetentionMultiplier);
  if (m_textureTimeout < 1)
    m_textureTimeout = 1;

  m_cacheCleanupInterval = renderingConfig.optInt("cacheCleanupIntervalMs").value(1500);
  if (m_cacheCleanupInterval < 50)
    m_cacheCleanupInterval = 50;
}

void WorldPainter::renderParticles(WorldRenderData& renderData, Particle::Layer layer) {
  const RectF particleRenderWindow = RectF::withSize(Vec2F(), Vec2F(m_camera.screenSize())).padded(m_particleRenderWindowPadding);
  List<Particle const*> const* particles = nullptr;
  if (layer == Particle::Layer::Back)
    particles = &m_backParticles;
  else if (layer == Particle::Layer::Middle)
    particles = &m_middleParticles;
  else if (layer == Particle::Layer::Front)
    particles = &m_frontParticles;
  if (!particles)
    return;

  uint64_t particleCap = m_particleRenderCapPerLayer;
  if (particleCap > 0 && m_particleAdaptiveBudgetFrameMs > 0.0f && m_previousFrameRenderMs > m_particleAdaptiveBudgetFrameMs) {
    float overloadRatio = m_previousFrameRenderMs / m_particleAdaptiveBudgetFrameMs;
    uint64_t scaledCap = (uint64_t)(particleCap / overloadRatio);
    particleCap = std::max(m_particleRenderCapPerLayerMin, scaledCap);
  }

  float frameCostForBudget = m_frameCostEmaMs > 0.0f ? m_frameCostEmaMs : m_previousFrameRenderMs;
  float layerBudgetMs = m_particleRenderLayerBudgetMs;
  if (layerBudgetMs > 0.0f && m_particleAdaptiveBudgetFrameMs > 0.0f && frameCostForBudget > m_particleAdaptiveBudgetFrameMs) {
    float scale = m_particleAdaptiveBudgetFrameMs / frameCostForBudget;
    layerBudgetMs = std::max(m_particleRenderLayerBudgetMinMs, layerBudgetMs * scale);
  }
  int64_t layerBudgetUs = (int64_t)std::llround((double)layerBudgetMs * 1000.0);
  int64_t layerStartUs = Time::monotonicMicroseconds();

  uint64_t renderedParticles = 0;
  for (auto particlePtr : *particles) {
    if (particleCap > 0 && renderedParticles >= particleCap)
      break;
    if (layerBudgetUs > 0 && (Time::monotonicMicroseconds() - layerStartUs) >= layerBudgetUs)
      break;

    Particle const& particle = *particlePtr;

    Vec2F position = m_camera.worldToScreen(particle.position);

    if (!particleRenderWindow.contains(position))
      continue;

    Vec2F size = Vec2F::filled(particle.size * m_camera.pixelRatio());

    if (particle.type == Particle::Type::Ember) {
      m_renderer->immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(),
        RectF(position - size / 2, position + size / 2),
        particle.color.toRgba(),
        particle.fullbright ? 0.0f : 1.0f);
      ++renderedParticles;

    } else if (particle.type == Particle::Type::Streak) {
      // Draw a rotated quad streaking in the direction the particle is coming from.
      // Sadly this looks awful.
      Vec2F dir = particle.velocity.normalized();
      Vec2F sideHalf = dir.rot90() * m_camera.pixelRatio() * particle.size / 2;
      float length = particle.length * m_camera.pixelRatio();
      Vec4B color = particle.color.toRgba();
      float lightMapMultiplier = particle.fullbright ? 0.0f : 1.0f;
      m_renderer->immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(),
        position - sideHalf,
        position + sideHalf,
        position - dir * length + sideHalf,
        position - dir * length - sideHalf,
        color, lightMapMultiplier);
      ++renderedParticles;

    } else if (particle.type == Particle::Type::Textured || particle.type == Particle::Type::Animated) {
      Drawable drawable;
      if (particle.type == Particle::Type::Textured)
        drawable = Drawable::makeImage(particle.image, 1.0f / TilePixels, true, Vec2F(0, 0));
      else
        drawable = particle.animation->drawable(1.0f / TilePixels);

      if (particle.flip && particle.flippable)
        drawable.scale(Vec2F(-1, 1));
      if (drawable.isImage() && particle.type != Particle::Type::Animated)
        drawable.imagePart().addDirectivesGroup(particle.directives, true);
      drawable.fullbright = particle.fullbright;
      drawable.color = particle.color;
      drawable.rotate(particle.rotation);
      drawable.scale(particle.size);
      drawable.translate(particle.position);
      drawDrawable(std::move(drawable));
      ++renderedParticles;

    } else if (particle.type == Particle::Type::Text) {
      Vec2F position = m_camera.worldToScreen(particle.position);
      int size = min(128.0f, round((float)m_textParticleFontSize * m_camera.pixelRatio() * particle.size));
      if (size > 0) {
        m_textPainter->setFontSize(size);
        m_textPainter->setFontColor(particle.color.toRgba());
        m_textPainter->setProcessingDirectives("");
        m_textPainter->setFont("");
        m_textPainter->renderText(particle.string, {position, HorizontalAnchor::HMidAnchor, VerticalAnchor::VMidAnchor});
        ++renderedParticles;
      }
    }
  }
}

void WorldPainter::renderBars(WorldRenderData& renderData) {
  auto offset = m_entityBarOffset;
  for (auto const& bar : renderData.overheadBars) {
    auto position = bar.entityPosition + offset;
    offset += m_entityBarSpacing;
    if (bar.icon) {
      auto iconDrawPosition = position - (m_entityBarSize / 2).round() + m_entityBarIconOffset;
      drawDrawable(Drawable::makeImage(*bar.icon, 1.0f / TilePixels, true, iconDrawPosition));
    }

    if (!bar.detailOnly) {
      auto fullBar = RectF({}, {m_entityBarSize.x() * bar.percentage, m_entityBarSize.y()});
      auto emptyBar = RectF({m_entityBarSize.x() * bar.percentage, 0.0f}, m_entityBarSize);
      auto fullColor = bar.color;
      auto emptyColor = Color::Black;

      drawDrawable(Drawable::makePoly(PolyF(emptyBar), emptyColor, position));
      drawDrawable(Drawable::makePoly(PolyF(fullBar), fullColor, position));
    }
  }
}

void WorldPainter::drawEntityLayer(List<Drawable>& drawableArena, size_t drawablesBegin, size_t drawablesCount, EntityHighlightEffect highlightEffect) {
  if (drawablesCount == 0)
    return;

  size_t drawablesEnd = drawablesBegin + drawablesCount;
  highlightEffect.level *= m_maxHighlightLevel;
  if (m_highlightDirectives.contains(highlightEffect.type) && highlightEffect.level > 0) {
    // first pass, draw underlay
    auto underlayDirectives = m_highlightDirectives[highlightEffect.type].first;
    if (!underlayDirectives.empty()) {
      for (size_t i = drawablesBegin; i < drawablesEnd; ++i) {
        auto& d = drawableArena[i];
        if (d.isImage()) {
          auto underlayDrawable = Drawable(d);
          underlayDrawable.fullbright = true;
          underlayDrawable.color = Color::rgbaf(1, 1, 1, highlightEffect.level * d.color.alphaF());
          underlayDrawable.imagePart().addDirectives(underlayDirectives, true);
          drawDrawable(std::move(underlayDrawable));
        }
      }
    }

    // second pass, draw main drawables and overlays
    auto overlayDirectives = m_highlightDirectives[highlightEffect.type].second;
    bool hasOverlay = !overlayDirectives.empty();
    for (size_t i = drawablesBegin; i < drawablesEnd; ++i) {
      auto& d = drawableArena[i];
      Maybe<Drawable> overlayDrawable;
      if (hasOverlay && d.isImage()) {
        overlayDrawable = Drawable(d);
        auto& overlay = *overlayDrawable;
        overlay.fullbright = true;
        overlay.color = Color::rgbaf(1, 1, 1, highlightEffect.level * d.color.alphaF());
        overlay.imagePart().addDirectives(overlayDirectives, true);
      }

      drawDrawable(std::move(d));
      if (overlayDrawable) {
        auto overlay = std::move(*overlayDrawable);
        drawDrawable(std::move(overlay));
      }
    }
  } else {
    for (size_t i = drawablesBegin; i < drawablesEnd; ++i)
      drawDrawable(std::move(drawableArena[i]));
  }
}

void WorldPainter::drawDrawable(Drawable drawable) {
  drawable.position = m_camera.worldToScreen(drawable.position);
  drawable.scale(m_camera.pixelRatio() * TilePixels, drawable.position);

  if (drawable.isLine())
    drawable.linePart().width *= m_camera.pixelRatio();

  if (m_skipDrawableCulling) {
    m_drawablePainter->drawDrawable(drawable);
    return;
  }

  if (m_worldScreenRect.intersects(drawable.boundBox(false)))
    m_drawablePainter->drawDrawable(drawable);
  else if (drawable.isImage()
      && (m_offscreenPreloadCapPerFrame == 0 || m_offscreenPreloadCount < m_offscreenPreloadCapPerFrame)
      && m_previousFrameRenderMs <= 16.7f
      && Random::randf() < m_preloadTextureChance) {
    m_assets->tryImage(drawable.imagePart().image);
    ++m_offscreenPreloadCount;
  }
}

void WorldPainter::drawDrawableSet(List<Drawable>& drawables) {
  for (Drawable& drawable : drawables)
    drawDrawable(std::move(drawable));
}

}
