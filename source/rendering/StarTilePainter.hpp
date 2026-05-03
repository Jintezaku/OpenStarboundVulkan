#pragma once

#include "StarTtlCache.hpp"
#include "StarWorldRenderData.hpp"
#include "StarMaterialRenderProfile.hpp"
#include "StarRenderer.hpp"
#include "StarWorldCamera.hpp"
#include "StarTileDrawer.hpp"
#include "StarListener.hpp"

namespace Star {

STAR_CLASS(Assets);
STAR_CLASS(MaterialDatabase);
STAR_CLASS(TilePainter);

class TilePainter : public TileDrawer {
public:
  // The rendered tiles are split and cached in chunks of RenderChunkSize x
  // RenderChunkSize.  This means that, around the border, there may be as many
  // as RenderChunkSize - 1 tiles rendered outside of the viewing area from
  // chunk alignment.  In addition to this, there is also a region around each
  // tile that is used for neighbor based rendering rules which has a max of
  // MaterialRenderProfileMaxNeighborDistance.  If the given tile data does not
  // extend RenderChunkSize + MaterialRenderProfileMaxNeighborDistance - 1
  // around the viewing area, then border chunks can continuously change hash,
  // and will be recomputed too often.
  static unsigned const RenderChunkSize = 16;
  static unsigned const BorderTileSize = RenderChunkSize + MaterialRenderProfileMaxNeighborDistance - 1;

  TilePainter(RendererPtr renderer);

  // Adjusts lighting levels for liquids.
  void adjustLighting(WorldRenderData& renderData) const;

  // Sets up chunk data for every chunk that intersects the rendering region
  // and prepares it for rendering.  Do not call cleanup in between calling
  // setup and each render method.
  void setup(WorldCamera const& camera, WorldRenderData& renderData);

  void renderBackground(WorldCamera const& camera);
  void renderMidground(WorldCamera const& camera);
  void renderLiquid(WorldCamera const& camera);
  void renderForeground(WorldCamera const& camera);

  // Clears frame-local render data.
  void cleanup();
  void cleanupCache();

private:
  typedef uint64_t QuadZLevel;
  typedef uint64_t ChunkHash;

  enum class TerrainLayer { Background, Midground, Foreground };

  struct LiquidInfo {
    TexturePtr texture;
    Vec4B color;
    Vec3F bottomLightMix;
    float textureMovementFactor;
  };

  typedef HashMap<TerrainLayer, HashMap<QuadZLevel, RenderBufferPtr>> TerrainChunk;
  typedef HashMap<LiquidId, RenderBufferPtr> LiquidChunk;

  typedef tuple<MaterialId, MaterialRenderPieceIndex, MaterialHue, bool> MaterialPieceTextureKey;
  typedef String AssetTextureKey;
  typedef Variant<MaterialPieceTextureKey, AssetTextureKey> TextureKey;

  struct TextureKeyHash {
    size_t operator()(TextureKey const& key) const;
  };

  // chunkIndex here is the index of the render chunk such that chunkIndex *
  // RenderChunkSize results in the coordinate of the lower left most tile in
  // the render chunk.

  pair<ChunkHash, ChunkHash> chunkHashes(WorldRenderData& renderData, Vec2I chunkIndex) const;
  pair<ChunkHash, ChunkHash> cachedChunkHashes(
      WorldRenderData& renderData,
      Vec2I chunkIndex,
      int hashRefreshStrideFrames,
      bool allowRefresh,
      bool* refreshed = nullptr);

  void renderTerrainChunks(WorldCamera const& camera, TerrainLayer terrainLayer);

  shared_ptr<TerrainChunk const> buildTerrainChunk(WorldRenderData& renderData, Vec2I chunkIndex);
  shared_ptr<LiquidChunk const> buildLiquidChunk(WorldRenderData& renderData, Vec2I chunkIndex);
  shared_ptr<TerrainChunk const> getTerrainChunk(WorldRenderData& renderData, Vec2I chunkIndex, ChunkHash terrainHash, int64_t& terrainBudgetMicros);
  shared_ptr<LiquidChunk const> getLiquidChunk(WorldRenderData& renderData, Vec2I chunkIndex, ChunkHash liquidHash, int64_t& liquidBudgetMicros);

  bool produceTerrainPrimitives(HashMap<QuadZLevel, List<RenderPrimitive>>& primitives,
      TerrainLayer terrainLayer, Vec2I const& pos, WorldRenderData const& renderData);
  void produceLiquidPrimitives(HashMap<LiquidId, List<RenderPrimitive>>& primitives, Vec2I const& pos, WorldRenderData const& renderData);

  float liquidDrawLevel(float liquidLevel) const;
  void refreshRenderConfig();

  List<LiquidInfo> m_liquids;

  RendererPtr m_renderer;
  TextureGroupPtr m_textureGroup;

  HashTtlCache<TextureKey, TexturePtr, TextureKeyHash> m_textureCache;
  HashTtlCache<pair<Vec2I, ChunkHash>, shared_ptr<TerrainChunk const>> m_terrainChunkCache;
  HashTtlCache<pair<Vec2I, ChunkHash>, shared_ptr<LiquidChunk const>> m_liquidChunkCache;
  struct CachedChunkHashes {
    ChunkHash terrainHash;
    ChunkHash liquidHash;
  };
  HashMap<Vec2I, CachedChunkHashes> m_cachedChunkHashes;
  HashMap<Vec2I, shared_ptr<TerrainChunk const>> m_lastTerrainChunks;
  HashMap<Vec2I, shared_ptr<LiquidChunk const>> m_lastLiquidChunks;

  List<RenderBufferPtr> m_backgroundTerrainBuffers;
  List<RenderBufferPtr> m_midgroundTerrainBuffers;
  List<RenderBufferPtr> m_foregroundTerrainBuffers;
  List<RenderBufferPtr> m_liquidBuffers;

  Mat3F m_cameraTransformation;

  Maybe<Vec2F> m_lastCameraCenter;
  Vec2F m_cameraPan;
  uint8_t m_terrainDamageHashLevelQuantization;
  uint8_t m_liquidHashLevelQuantization;
  bool m_enableAdaptiveChunkBuildBudget;
  int64_t m_baseTerrainChunkBuildBudgetMicros;
  int64_t m_baseLiquidChunkBuildBudgetMicros;
  int64_t m_minTerrainChunkBuildBudgetMicros;
  int64_t m_minLiquidChunkBuildBudgetMicros;
  double m_chunkBuildBudgetOverloadStartMs;
  double m_chunkBuildBudgetOverloadMaxMs;
  double m_setupCostEmaMs;
  double m_setupCostSmoothing;
  bool m_enableAdaptiveChunkHashCadence;
  int m_chunkHashRefreshBaseStrideFrames;
  int m_chunkHashRefreshMaxStrideFrames;
  int m_chunkHashFarStrideFrames;
  bool m_enableChunkHashRefreshBudget;
  int m_chunkHashRefreshBudgetPerFrame;
  bool m_enableDistanceWeightedChunkHashCadence;
  bool m_enableVisibleChunkPriority;
  int m_criticalChunkSyncBuildsPerFrame;
  bool m_enableChunkPrefetch;
  int m_chunkPrefetchRing;
  int m_chunkPrefetchPerFrame;
  uint64_t m_setupFrameIndex;
  Maybe<uint64_t> m_cachedWorldRenderGeneration;
  Maybe<Vec2U> m_cachedWorldSize;

  TrackerListenerPtr m_reloadTracker;
};

}
