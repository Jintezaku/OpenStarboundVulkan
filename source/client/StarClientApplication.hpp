#pragma once

#include "StarUniverseServer.hpp"
#include "StarUniverseClient.hpp"
#include "StarWorldPainter.hpp"
#include "StarGameTypes.hpp"
#include "StarMainInterface.hpp"
#include "StarMainMixer.hpp"
#include "StarTitleScreen.hpp"
#include "StarErrorScreen.hpp"
#include "StarCinematic.hpp"
#include "StarKeyBindings.hpp"
#include "StarMainApplication.hpp"

namespace Star {

STAR_CLASS(Input);
STAR_CLASS(Voice);

class ClientApplication : public Application {
public:
  void setPostProcessLayerPasses(String const& layer, unsigned const& passes);
  void setPostProcessGroupEnabled(String const& group, bool const& enabled, Maybe<bool> const& save);
  bool postProcessGroupEnabled(String const& group);
  Json postProcessGroups();
  virtual unsigned framesSkipped() const override;

protected:
  virtual void startup(StringList const& cmdLineArgs) override;
  virtual void shutdown() override;

  virtual void applicationInit(ApplicationControllerPtr appController) override;
  virtual void renderInit(RendererPtr renderer) override;

  virtual void windowChanged(WindowMode windowMode, Vec2U screenSize) override;

  virtual void processInput(InputEvent const& event) override;

  virtual void update() override;
  virtual void render() override;

  virtual void getAudioData(int16_t* stream, size_t len) override;

private:
  enum class MainAppState {
    Quit,
    Startup,
    SteamFlatpakWarning,
    Mods,
    ModsWarning,
    Splash,
    Error,
    Title,
    SinglePlayer,
    MultiPlayer
  };

  struct PendingMultiPlayerConnection {
    Variant<P2PNetworkingPeerId, HostAddressWithPort> server;
    String account;
    String password;
    bool forceLegacy;
  };
  
  struct PostProcessGroup {
    bool enabled;
  };
  
  struct PostProcessLayer {
    List<String> effects;
    unsigned passes;
    PostProcessGroup* group;
  };

  enum class BenchmarkPhase {
    Disabled,
    WaitingForWorld,
    Warmup,
    AreaSweepPrimary,
    WarpToShip,
    AreaSweepShip,
    WarpToOrbitedWorld,
    AreaSweepOrbitedWorld,
    AssetLoadSweep,
    Finalize,
    Completed
  };

  struct BenchmarkFrameSample {
    double timeSeconds = 0.0;
    double frameMs = 0.0;
    double worldClientMs = 0.0;
    double worldPainterMs = 0.0;
    double worldTotalMs = 0.0;
    double interfaceMs = 0.0;
    double renderFps = 0.0;
    uint64_t loadedSectors = 0;
    uint64_t deferredSectorUnloads = 0;
    String phase;
    String worldId;
  };

  struct BenchmarkWarpEvent {
    String label;
    String action;
    String fromWorldId;
    String toWorldId;
    double requestedAtSeconds = 0.0;
    double completedAtSeconds = 0.0;
    double durationMs = 0.0;
    bool completed = false;
    String error;
  };

  struct BenchmarkSlowAssetSample {
    String path;
    String loadType;
    double loadMs = 0.0;
    bool success = true;
    String error;
  };

  struct BenchmarkAssetTypeStats {
    uint64_t attempted = 0;
    uint64_t succeeded = 0;
    uint64_t failed = 0;
    double totalMs = 0.0;
    double maxMs = 0.0;
  };

  struct BenchmarkState {
    bool enabled = false;
    bool running = false;
    bool completed = false;
    bool autoQuit = false;
    bool forceCameraSweep = true;
    bool assetScanEnabled = true;
    bool stressMode = false;
    bool stressPrepared = false;
    double stressPrepareNotBeforeSeconds = 0.0;
    bool stressPreWarpRequested = false;
    double stressPreWarpRetryAtSeconds = 0.0;
    bool stressForceZoomOut = true;
    bool stressUseExtendedDurations = true;
    bool isolateStorage = true;
    bool requireLateGameWorld = true;
    bool preferTerrestrialLateGameWorld = true;
    double lateGameThreatLevel = 6.0;
    double warmupSeconds = 5.0;
    double areaPrimarySeconds = 35.0;
    double areaShipSeconds = 20.0;
    double areaOrbitedSeconds = 20.0;
    double warpTimeoutSeconds = 45.0;
    double cameraAmplitudeTiles = 96.0;
    double cameraFrequencyHz = 0.22;
    double areaSettleSeconds = 1.0;
    double areaPrewarmSweepSeconds = 5.0;
    uint64_t assetSampleCount = 5000;
    uint64_t maxSlowAssets = 64;
    uint64_t minLoadedSectors = 24;
    bool sampleOnlyReadySectors = true;
    uint64_t stressItemDrops = 160;
    uint64_t stressNpcCount = 24;
    uint64_t stressMonsterCount = 48;
    uint64_t stressLiquidBursts = 72;
    double stressAiWaveIntervalSeconds = 3.2;
    double stressItemWaveIntervalSeconds = 2.4;
    double stressTerrainPulseIntervalSeconds = 1.15;
    double stressLiquidPulseIntervalSeconds = 1.75;
    double stressExplosionPulseIntervalSeconds = 5.0;
    double stressJumpIntervalSeconds = 1.35;
    double stressWeatherPulseIntervalSeconds = 22.0;
    String outputPath;

    double stressNextAiWaveAtSeconds = 0.0;
    double stressNextItemWaveAtSeconds = 0.0;
    double stressNextTerrainPulseAtSeconds = 0.0;
    double stressNextLiquidPulseAtSeconds = 0.0;
    double stressNextExplosionPulseAtSeconds = 0.0;
    double stressNextJumpAtSeconds = 0.0;
    double stressNextWeatherPulseAtSeconds = 0.0;
    double stressNextEntityTrimAtSeconds = 0.0;
    double scenarioNextActionAtSeconds = 0.0;
    uint64_t scenarioPreparationAttempts = 0;
    bool stressTerrainRebuildPass = false;
    bool stressWeatherForceEnabled = true;
    StringList stressWeatherCycle;
    size_t stressWeatherCycleIndex = 0;
    bool scenarioPlayerReady = false;
    bool scenarioLateGameReady = false;
    Maybe<Uuid> scenarioPlayerUuid;
    Maybe<CelestialCoordinate> lateGameTargetCoordinate;
    String lateGameTargetWorldId;
    String benchmarkStorageDirectory;
    String benchmarkBootConfigPath;
    Maybe<Vec2I> stressAnchorTile;
    Maybe<Vec2F> stressCommandAimPosition;
    MaterialId stressRebuildForeground = StructureMaterialId;
    MaterialId stressRebuildBackground = StructureMaterialId;
    LiquidId stressLiquidPrimary = EmptyLiquidId;
    LiquidId stressLiquidSecondary = EmptyLiquidId;
    LiquidId stressLiquidTertiary = EmptyLiquidId;
    uint64_t stressAiWaves = 0;
    uint64_t stressItemWaves = 0;
    uint64_t stressTerrainPulses = 0;
    uint64_t stressLiquidPulses = 0;
    uint64_t stressExplosionPulses = 0;
    uint64_t stressJumpPulses = 0;
    uint64_t stressWeatherPulses = 0;
    uint64_t stressCommandsIssued = 0;
    uint64_t stressTerrainTilesDamaged = 0;
    uint64_t stressTerrainTilesRebuilt = 0;
    uint64_t stressLiquidTileWrites = 0;
    uint64_t stressTrimmedItemDrops = 0;
    uint64_t stressTrimmedNpcs = 0;
    uint64_t stressTrimmedMonsters = 0;

    BenchmarkPhase phase = BenchmarkPhase::Disabled;
    uint64_t startedAtMonotonicUs = 0;
    int64_t startedAtEpochMs = 0;
    double startedAtSeconds = 0.0;
    double phaseStartedAtSeconds = 0.0;
    uint64_t frameCount = 0;
    uint64_t framesOver16Ms = 0;
    uint64_t framesOver33Ms = 0;
    uint64_t framesOver50Ms = 0;
    uint64_t framesOver100Ms = 0;
    uint64_t framesWithoutWorldTiming = 0;
    double areaDistanceSweptTiles = 0.0;
    double assetSweepTotalMs = 0.0;
    uint64_t assetSweepAttempted = 0;
    uint64_t assetSweepSucceeded = 0;
    uint64_t assetSweepFailed = 0;
    String lastWorldId;
    String completionReason;
    bool areaSweepReadyForSampling = false;
    double areaSweepReadySinceSeconds = 0.0;
    double areaSweepEnteredAtSeconds = 0.0;

    Maybe<BenchmarkWarpEvent> pendingWarpEvent;
    List<String> visitedWorldIds;
    List<BenchmarkWarpEvent> warpEvents;
    List<BenchmarkFrameSample> frameSamples;
    StringMap<BenchmarkAssetTypeStats> assetTypeStats;
    StringMap<uint64_t> assetCatalogCounts;
    List<BenchmarkSlowAssetSample> slowAssetSamples;
    List<String> warnings;
  };

  void renderReload();
  void refreshInterfaceScale();

  void changeState(MainAppState newState);
  void setError(String const& error);
  void setError(String const& error, std::exception const& e);

  void loadMods();
  void updateSteamFlatpakWarning(float dt);
  void updateMods(float dt);
  void updateModsWarning(float dt);
  void updateSplash(float dt);
  void updateError(float dt);
  void updateTitle(float dt);
  void updateRunning(float dt);

  bool isActionTaken(InterfaceAction action) const;
  bool isActionTakenEdge(InterfaceAction action) const;

  void updateCamera(float dt, WorldClientPtr const& worldClient);
  void parseBenchmarkArgs(StringList& cmdLineArgs);
  void benchmarkEnterPhase(BenchmarkPhase phase);
  bool benchmarkPhaseElapsed(double seconds) const;
  String benchmarkPhaseName(BenchmarkPhase phase) const;
  void updateBenchmark(float dt, WorldClientPtr const& worldClient);
  void benchmarkRequestWarp(String label, WarpAction const& action, String actionName);
  void benchmarkRunAssetLoadSweep();
  void benchmarkPrepareStressScene();
  void benchmarkResetStressActions();
  void benchmarkUpdateStressActions(WorldClientPtr const& worldClient, String const& worldId);
  void benchmarkStressPlayerMovement(WorldClientPtr const& worldClient, double nowSeconds);
  void benchmarkStressSpawnAiWave();
  void benchmarkStressSpawnItemWave();
  void benchmarkStressTerrainPulse(WorldClientPtr const& worldClient);
  void benchmarkStressLiquidPulse(WorldClientPtr const& worldClient);
  void benchmarkStressExplosionPulse(WorldClientPtr const& worldClient);
  void benchmarkTrimStressEntities(WorldClientPtr const& worldClient);
  void benchmarkEnsureStressWeatherCycle();
  String benchmarkNextStressWeather();
  Vec2I benchmarkStressAnchorTile(WorldClientPtr const& worldClient);
  Vec2F benchmarkStressCommandAim(WorldClientPtr const& worldClient);
  Vec2F benchmarkStressSpawnPosition(WorldClientPtr const& worldClient);
  void benchmarkSyncStressCommandAim(WorldClientPtr const& worldClient);
  bool benchmarkExecuteAtStressAnchor(function<void(WorldServer*, PlayerPtr const&, Vec2F const&)> const& action);
  bool benchmarkSpawnItemDirect(String const& itemName, uint64_t amount = 1);
  bool benchmarkSpawnNpcDirect(String const& species, String const& npcType, float level);
  bool benchmarkSpawnMonsterDirect(String const& monsterType, float level);
  bool benchmarkSpawnLiquidDirect(String const& liquidName, float quantity);
  bool benchmarkIssueCommand(String const& command);
  void benchmarkRecordFrameSample(WorldClientPtr const& worldClient, uint64_t frameUs, uint64_t worldClientUs, uint64_t worldPainterUs, uint64_t worldTotalUs, uint64_t interfaceUs);
  void benchmarkFinalize(String reason);
  void benchmarkConfigureIsolatedStorage(StringList& startupArgs);
  bool benchmarkEnsureScenarioPlayer();
  void benchmarkConfigureScenarioPlayer(PlayerPtr const& player);
  bool benchmarkEnsureLateGameWorld(WorldClientPtr const& worldClient, String const& worldId);
  Maybe<CelestialCoordinate> benchmarkFindLateGameWorld();

  RootUPtr m_root;
  ThreadFunction<void> m_rootLoader;
  CallbackListenerPtr m_reloadListener;

  MainAppState m_state = MainAppState::Startup;

  // Valid after applicationInit is called
  MainMixerPtr m_mainMixer;
  GuiContextPtr m_guiContext;
  InputPtr m_input;
  VoicePtr m_voice;

  // Valid after renderInit is called the first time
  CinematicPtr m_cinematicOverlay;
  ErrorScreenPtr m_errorScreen;

  // Valid if main app state >= Title
  PlayerStoragePtr m_playerStorage;
  StatisticsPtr m_statistics;
  UniverseClientPtr m_universeClient;
  TitleScreenPtr m_titleScreen;

  // Valid if main app state > Title
  PlayerPtr m_player;
  WorldPainterPtr m_worldPainter;
  WorldRenderData m_renderData;
  MainInterfacePtr m_mainInterface;
  
  StringMap<PostProcessGroup> m_postProcessGroups;
  List<PostProcessLayer> m_postProcessLayers;
  StringMap<size_t> m_labelledPostProcessLayers;

  // Valid if main app state == SinglePlayer
  UniverseServerPtr m_universeServer;

  float m_cameraXOffset = 0.0f;
  float m_cameraYOffset = 0.0f;
  bool m_snapBackCameraOffset = false;
  float m_cameraOffsetDownTime = 0.f;
  Vec2F m_cameraPositionSmoother;
  Vec2F m_cameraSmoothDelta;
  int m_cameraZoomDirection = 0;

  unsigned m_framesSkipped = 0;
  float m_minInterfaceScale = 2;
  float m_maxInterfaceScale = 3;
  Vec2F m_crossoverRes;

  bool m_controllerInput;
  Vec2F m_controllerLeftStick;
  Vec2F m_controllerRightStick;
  List<KeyDownEvent> m_heldKeyEvents;
  List<KeyDownEvent> m_edgeKeyEvents;

  Maybe<PendingMultiPlayerConnection> m_pendingMultiPlayerConnection;
  Maybe<HostAddressWithPort> m_currentRemoteJoin;
  int64_t m_timeSinceJoin = 0;

  ByteArray m_immediateFont;

  bool m_loggedUGCCheck;
  BenchmarkState m_benchmark;
};

}
