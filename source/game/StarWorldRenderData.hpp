#pragma once

#include "StarImage.hpp"
#include "StarWorldTiles.hpp"
#include "StarEntityRenderingTypes.hpp"
#include "StarSkyRenderData.hpp"
#include "StarParallax.hpp"
#include "StarParticle.hpp"
#include "StarWeatherTypes.hpp"
#include "StarEntity.hpp"
#include "StarThread.hpp"
#include "StarCellularLighting.hpp"

namespace Star {

using LayerDrawables = pair<EntityRenderLayer, List<Drawable>>;

struct EntityLayerDrawables {
  EntityRenderLayer layer;
  EntityHighlightEffect highlightEffect;
  size_t drawablesBegin;
  size_t drawablesCount;
};


struct WorldRenderData {
  void clear();

  WorldGeometry geometry;
  uint64_t worldRenderGeneration = 0;

  Vec2I tileMinPosition;
  RenderTileArray tiles;
  Vec2I lightMinPosition;
  Lightmap lightMap;

  List<EntityLayerDrawables> entityLayerDrawables;
  List<Drawable> entityDrawablesArena;
  uint64_t entityDrawableCount = 0;
  List<Particle> const* particles;

  List<OverheadBar> overheadBars;
  List<Drawable> nametags;

  List<Drawable> backgroundOverlays;
  List<Drawable> foregroundOverlays;

  List<ParallaxLayer> parallaxLayers;

  SkyRenderData skyRenderData;

  bool isFullbright = false;
  float dimLevel = 0.0f;
  Vec3B dimColor;
};

inline void WorldRenderData::clear() {
  tiles.resize({0, 0}); // keep reserved

  entityLayerDrawables.clear();
  entityDrawablesArena.clear();
  entityDrawableCount = 0;
  particles = nullptr;
  overheadBars.clear();
  nametags.clear();
  backgroundOverlays.clear();
  foregroundOverlays.clear();
  parallaxLayers.clear();
}

}
