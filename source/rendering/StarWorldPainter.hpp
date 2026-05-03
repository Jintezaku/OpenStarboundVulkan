#pragma once

#include "StarWorldRenderData.hpp"
#include "StarTilePainter.hpp"
#include "StarEnvironmentPainter.hpp"
#include "StarTextPainter.hpp"
#include "StarDrawablePainter.hpp"
#include "StarRenderer.hpp"
#include "StarListener.hpp"

namespace Star {

STAR_CLASS(WorldPainter);

// Will update client rendering window internally
class WorldPainter {
public:
  WorldPainter();

  void renderInit(RendererPtr renderer);

  void setCameraPosition(WorldGeometry const& worldGeometry, Vec2F const& position);

  WorldCamera& camera();

  void update(float dt);
  void render(WorldRenderData& renderData, function<bool()> lightWaiter);
  void adjustLighting(WorldRenderData& renderData);

private:
  void refreshRenderConfig();
  void renderParticles(WorldRenderData& renderData, Particle::Layer layer);
  void renderBars(WorldRenderData& renderData);

  void drawEntityLayer(List<Drawable>& drawableArena, size_t drawablesBegin, size_t drawablesCount, EntityHighlightEffect highlightEffect = EntityHighlightEffect());

  void drawDrawable(Drawable drawable);
  void drawDrawableSet(List<Drawable>& drawable);

  WorldCamera m_camera;

  RendererPtr m_renderer;

  TextPainterPtr m_textPainter;
  DrawablePainterPtr m_drawablePainter;
  EnvironmentPainterPtr m_environmentPainter;
  TilePainterPtr m_tilePainter;

  Json m_highlightConfig;
  Map<EntityHighlightEffectType, pair<Directives, Directives>> m_highlightDirectives;
  float m_maxHighlightLevel;

  Vec2F m_entityBarOffset;
  Vec2F m_entityBarSpacing;
  Vec2F m_entityBarSize;
  Vec2F m_entityBarIconOffset;

  // Updated every frame

  AssetsConstPtr m_assets;
  RectF m_worldScreenRect;
  RectF m_worldCoarseCullRect;

  Vec2F m_previousCameraCenter;
  Vec2F m_parallaxWorldPosition;

  float m_preloadTextureChance;
  float m_lightMapMultiplier;
  int m_textParticleFontSize;
  int m_particleRenderWindowPadding;
  uint64_t m_particleRenderCapPerLayer;
  uint64_t m_particleRenderCapPerLayerMin;
  float m_particleAdaptiveBudgetFrameMs;
  uint64_t m_drawableCullBypassThreshold;
  float m_drawableCullBypassMaxFrameMs;
  float m_drawableWorldCoarseCullPadding;
  bool m_skipDrawableCulling;
  int64_t m_textureTimeout;
  int64_t m_nextCleanupTime;
  int64_t m_cacheCleanupInterval;
  uint8_t m_cacheCleanupPhase;
  float m_previousFrameRenderMs;

  List<Particle const*> m_backParticles;
  List<Particle const*> m_middleParticles;
  List<Particle const*> m_frontParticles;

  TrackerListenerPtr m_reloadTracker;
};

}
