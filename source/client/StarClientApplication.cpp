#include "StarClientApplication.hpp"
#include "StarConfiguration.hpp"
#include "StarJsonExtra.hpp"
#include "StarFile.hpp"
#include "StarEncode.hpp"
#include "StarLogging.hpp"
#include "StarJsonExtra.hpp"
#include "StarSteamDeck.hpp"
#include "StarRoot.hpp"
#include "StarVersion.hpp"
#include "StarPlayer.hpp"
#include "StarPlayerStorage.hpp"
#include "StarPlayerLog.hpp"
#include "StarAssets.hpp"
#include "StarWorldTemplate.hpp"
#include "StarWorldClient.hpp"
#include "StarMaterialDatabase.hpp"
#include "StarLiquidsDatabase.hpp"
#include "StarCelestialDatabase.hpp"
#include "StarWorldParameters.hpp"
#include "StarTileDamage.hpp"
#include "StarTileModification.hpp"
#include "StarItemDatabase.hpp"
#include "StarItemDrop.hpp"
#include "StarMonster.hpp"
#include "StarMonsterDatabase.hpp"
#include "StarNpc.hpp"
#include "StarNpcDatabase.hpp"
#include "StarRootLoader.hpp"
#include "StarInput.hpp"
#include "StarVoice.hpp"
#include "StarCurve25519.hpp"
#include "StarInterpolation.hpp"
#include "StarLexicalCast.hpp"
#include "StarText.hpp"

#include "StarCameraLuaBindings.hpp"
#include "StarCelestialLuaBindings.hpp"
#include "StarClipboardLuaBindings.hpp"
#include "StarInputLuaBindings.hpp"
#include "StarInterfaceLuaBindings.hpp"
#include "StarLuaHttpBindings.hpp"
#include "StarRenderingLuaBindings.hpp"
#include "StarTeamClientLuaBindings.hpp"
#include "StarVoiceLuaBindings.hpp"
#include "StarHttpTrustDialog.hpp"
#include "StarMainInterfaceTypes.hpp"
#include "StarClientCommandProcessor.hpp"

#include "imgui.h"
#include "imgui_freetype.h"

#include <algorithm>
#include <cmath>

#if defined STAR_SYSTEM_WINDOWS
#include <windows.h>
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;
#endif // graphics driver is told by these exports to default to the dedicated GPU

namespace Star {

Json const AdditionalAssetsSettings = Json::parseJson(R"JSON(
    {
      "missingImage" : "/assetmissing.png",
      "missingAudio" : "/assetmissing.wav"
    }
  )JSON");

Json const AdditionalDefaultConfiguration = Json::parseJson(R"JSON(
    {
      "configurationVersion" : {
        "client" : 8
      },

      "allowAssetsMismatch" : false,
      "vsync" : true,
      "limitTextureAtlasSize" : false,
      "useMultiTexturing" : true,
      "audioChannelSeparation" : [-25, 25],

      "sfxVol" : 100,
      "instrumentVol" : 100,
      "musicVol" : 70,
      "hardwareCursor" : true,
      "windowedResolution" : [1000, 600],
      "fullscreenResolution" : [1920, 1080],
      "fullscreen" : false,
      "borderless" : false,
      "maximized" : true,
      "antiAliasing" : false,
      "vulkanPipelineCache" : true,
      "vulkanTransferQueue" : true,
      "vulkanStaticCommandBuffers" : true,
      "vulkanLowLatencyPresent" : true,
      "vulkanFramesInFlight" : 3,
      "zoomLevel" : 3.0,
      "cameraSpeedFactor" : 1.0,
      "interfaceScale" : 0,
      "controllerInput" : false,
      "speechBubbles" : true,

      "title" : {
        "multiPlayerAddress" : "",
        "multiPlayerPort" : "",
        "multiPlayerAccount" : "",
        "multiPlayerForceLegacy" : false
      },

      "bindings" : {
        "PlayerUp" :  [ { "type" : "key", "value" : "W", "mods" : [] } ],
        "PlayerDown" :  [ { "type" : "key", "value" : "S", "mods" : [] } ],
        "PlayerLeft" :  [ { "type" : "key", "value" : "A", "mods" : [] } ],
        "PlayerRight" :  [ { "type" : "key", "value" : "D", "mods" : [] } ],
        "PlayerJump" :  [ { "type" : "key", "value" : "Space", "mods" : [] } ],
        "PlayerDropItem" :  [ { "type" : "key", "value" : "Q", "mods" : [] } ],
        "PlayerInteract" :  [ { "type" : "key", "value" : "E", "mods" : [] } ],
        "PlayerShifting" :  [ { "type" : "key", "value" : "RShift", "mods" : [] }, { "type" : "key", "value" : "LShift", "mods" : [] } ],
        "PlayerTechAction1" :  [ { "type" : "key", "value" : "F", "mods" : [] } ],
        "PlayerTechAction2" :  [],
        "PlayerTechAction3" :  [],
        "EmoteBlabbering" :  [ { "type" : "key", "value" : "Right", "mods" : ["LCtrl", "LShift"] } ],
        "EmoteShouting" :  [ { "type" : "key", "value" : "Up", "mods" : ["LCtrl", "LAlt"] } ],
        "EmoteHappy" :  [ { "type" : "key", "value" : "Up", "mods" : [] } ],
        "EmoteSad" :  [ { "type" : "key", "value" : "Down", "mods" : [] } ],
        "EmoteNeutral" :  [ { "type" : "key", "value" : "Left", "mods" : [] } ],
        "EmoteLaugh" :  [ { "type" : "key", "value" : "Left", "mods" : [ "LCtrl" ] } ],
        "EmoteAnnoyed" :  [ { "type" : "key", "value" : "Right", "mods" : [] } ],
        "EmoteOh" :  [ { "type" : "key", "value" : "Right", "mods" : [ "LCtrl" ] } ],
        "EmoteOooh" :  [ { "type" : "key", "value" : "Down", "mods" : [ "LCtrl" ] } ],
        "EmoteBlink" :  [ { "type" : "key", "value" : "Up", "mods" : [ "LCtrl" ] } ],
        "EmoteWink" :  [ { "type" : "key", "value" : "Up", "mods" : ["LCtrl", "LShift"] } ],
        "EmoteEat" :  [ { "type" : "key", "value" : "Down", "mods" : ["LCtrl", "LShift"] } ],
        "EmoteSleep" :  [ { "type" : "key", "value" : "Left", "mods" : ["LCtrl", "LShift"] } ],
        "ShowLabels" :  [ { "type" : "key", "value" : "RAlt", "mods" : [] }, { "type" : "key", "value" : "LAlt", "mods" : [] } ],
        "CameraShift" :  [ { "type" : "key", "value" : "RCtrl", "mods" : [] }, { "type" : "key", "value" : "LCtrl", "mods" : [] } ],
        "TitleBack" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "CinematicSkip" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "CinematicNext" :  [ { "type" : "key", "value" : "Right", "mods" : [] }, { "type" : "key", "value" : "Return", "mods" : [] } ],
        "GuiClose" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "GuiShifting" :  [ { "type" : "key", "value" : "RShift", "mods" : [] }, { "type" : "key", "value" : "LShift", "mods" : [] } ],
        "KeybindingCancel" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "KeybindingClear" :  [ { "type" : "key", "value" : "Del", "mods" : [] }, { "type" : "key", "value" : "Backspace", "mods" : [] } ],
        "ChatPageUp" :  [ { "type" : "key", "value" : "PageUp", "mods" : [] } ],
        "ChatPageDown" :  [ { "type" : "key", "value" : "PageDown", "mods" : [] } ],
        "ChatPreviousLine" :  [ { "type" : "key", "value" : "Up", "mods" : [] } ],
        "ChatNextLine" :  [ { "type" : "key", "value" : "Down", "mods" : [] } ],
        "ChatSendLine" :  [ { "type" : "key", "value" : "Return", "mods" : [] } ],
        "ChatBegin" :  [ { "type" : "key", "value" : "Return", "mods" : [] } ],
        "ChatBeginCommand" :  [ { "type" : "key", "value" : "/", "mods" : [] } ],
        "ChatStop" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "InterfaceHideHud" :  [ { "type" : "key", "value" : "F1", "mods" : [] } ],
        "InterfaceChangeBarGroup" :  [ { "type" : "key", "value" : "X", "mods" : [] } ],
        "InterfaceDeselectHands" :  [ { "type" : "key", "value" : "Z", "mods" : [] } ],
        "InterfaceBar1" :  [ { "type" : "key", "value" : "1", "mods" : [] } ],
        "InterfaceBar2" :  [ { "type" : "key", "value" : "2", "mods" : [] } ],
        "InterfaceBar3" :  [ { "type" : "key", "value" : "3", "mods" : [] } ],
        "InterfaceBar4" :  [ { "type" : "key", "value" : "4", "mods" : [] } ],
        "InterfaceBar5" :  [ { "type" : "key", "value" : "5", "mods" : [] } ],
        "InterfaceBar6" :  [ { "type" : "key", "value" : "6", "mods" : [] } ],
        "InterfaceBar7" :  [],
        "InterfaceBar8" :  [],
        "InterfaceBar9" :  [],
        "InterfaceBar10" :  [],
        "EssentialBar1" :  [ { "type" : "key", "value" : "R", "mods" : [] } ],
        "EssentialBar2" :  [ { "type" : "key", "value" : "T", "mods" : [] } ],
        "EssentialBar3" :  [ { "type" : "key", "value" : "Y", "mods" : [] } ],
        "EssentialBar4" :  [ { "type" : "key", "value" : "N", "mods" : [] } ],
        "InterfaceRepeatCommand" :  [ { "type" : "key", "value" : "P", "mods" : [] } ],
        "InterfaceToggleFullscreen" :  [ { "type" : "key", "value" : "F11", "mods" : [] } ],
        "InterfaceReload" :  [],
        "InterfaceEscapeMenu" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "InterfaceInventory" :  [ { "type" : "key", "value" : "I", "mods" : [] } ],
        "InterfaceCodex" :  [ { "type" : "key", "value" : "L", "mods" : [] } ],
        "InterfaceQuest" :  [ { "type" : "key", "value" : "J", "mods" : [] } ],
        "InterfaceCrafting" :  [ { "type" : "key", "value" : "C", "mods" : [] } ]
      }
    }
)JSON");

namespace {

bool isVulkanRendererId(String const& rendererId) {
  return rendererId.toLower().contains("vulkan");
}

Json applyVulkanRendererConfigOverrides(Json rendererConfig, ConfigurationPtr const& configuration) {
  bool vsyncEnabled = configuration->get("vsync").optBool().value(true);
  bool lowLatencyPresent = configuration->get("vulkanLowLatencyPresent").optBool().value(true);
  bool staticCommandBuffers = configuration->get("vulkanStaticCommandBuffers").optBool().value(true);
  bool enablePipelineCache = configuration->get("vulkanPipelineCache").optBool().value(true);
  bool enableTransferQueue = configuration->get("vulkanTransferQueue").optBool().value(true);
  auto framesInFlightValue = std::clamp(configuration->get("vulkanFramesInFlight").optInt().value((int64_t)3), (int64_t)2, (int64_t)4);
  uint32_t framesInFlight = (uint32_t)framesInFlightValue;

  String presentMode = "fifo";
  if (!vsyncEnabled)
    presentMode = lowLatencyPresent ? "mailbox" : "immediate";

  rendererConfig = rendererConfig.set("presentMode", presentMode);
  rendererConfig = rendererConfig.set("framesInFlight", framesInFlight);
  rendererConfig = rendererConfig.set("staticCommandBuffers", staticCommandBuffers);
  rendererConfig = rendererConfig.set("enablePipelineCache", enablePipelineCache);
  rendererConfig = rendererConfig.set("enableTransferQueue", enableTransferQueue);

  return rendererConfig;
}

Maybe<String> parsePlusArgValue(String const& argument, String const& key) {
  String plainArgument = strf("+{}", key);
  if (argument.equalsIgnoreCase(plainArgument))
    return String();

  for (char splitToken : {'=', ':'}) {
    String prefix = strf("+{}{}", key, splitToken);
    if (argument.beginsWith(prefix, String::CaseInsensitive))
      return argument.substr(prefix.size());
  }

  return {};
}

bool parseBoolLike(String value, bool defaultValue, bool emptyValue) {
  if (value.empty())
    return emptyValue;

  value = value.trim().toLower();
  if (value == "1" || value == "true" || value == "yes" || value == "on")
    return true;
  if (value == "0" || value == "false" || value == "no" || value == "off")
    return false;
  return defaultValue;
}

double parseDoubleLike(String value, double defaultValue, double minimum, double maximum) {
  if (value.empty())
    return defaultValue;

  if (auto parsed = maybeLexicalCast<double>(value.trim()))
    return std::clamp(*parsed, minimum, maximum);

  return defaultValue;
}

uint64_t parseUIntLike(String value, uint64_t defaultValue, uint64_t minimum, uint64_t maximum) {
  if (value.empty())
    return defaultValue;

  if (auto parsed = maybeLexicalCast<int64_t>(value.trim())) {
    if (*parsed < 0)
      return defaultValue;
    return std::clamp<uint64_t>((uint64_t)*parsed, minimum, maximum);
  }

  return defaultValue;
}

double monotonicSeconds() {
  return (double)Time::monotonicMicroseconds() / 1000000.0;
}

bool benchmarkIsShipWorldId(String const& worldId) {
  return worldId.beginsWith("ClientShipWorld:", String::CaseInsensitive);
}

String benchmarkSourceBootConfigPath(StringList const& args) {
  for (size_t i = 0; i < args.size(); ++i) {
    String const& arg = args.at(i);
    if (arg.equals("-bootconfig") || arg.equals("--bootconfig")) {
      if (i + 1 < args.size())
        return args.at(i + 1);
      return "sbinit.config";
    }

    if (arg.beginsWith("-bootconfig=", String::CaseInsensitive))
      return arg.substr(String("-bootconfig=").size());
    if (arg.beginsWith("--bootconfig=", String::CaseInsensitive))
      return arg.substr(String("--bootconfig=").size());
  }

  return "sbinit.config";
}

}

namespace {
  unsigned const SteamDeckProfileVersion = 1;
  Vec2U const SteamDeckNativeResolution = Vec2U(1280, 800);

  void applySteamDeckProfile(ConfigurationPtr const& configuration) {
    if (!isSteamDeck())
      return;

    if (configuration->get("steamDeckProfileVersion").optUInt().value(0) >= SteamDeckProfileVersion)
      return;

    bool changed = false;
    auto setIfDifferent = [&](String const& key, Json const& value) {
      if (configuration->get(key) != value) {
        configuration->set(key, value);
        changed = true;
      }
    };

    auto deckResolution = jsonFromVec2U(SteamDeckNativeResolution);

    auto windowedResolution = jsonToVec2U(configuration->get("windowedResolution", deckResolution));
    if (windowedResolution[0] < SteamDeckNativeResolution[0] || windowedResolution[1] < SteamDeckNativeResolution[1])
      setIfDifferent("windowedResolution", deckResolution);

    auto fullscreenResolution = jsonToVec2U(configuration->get("fullscreenResolution", deckResolution));
    if (fullscreenResolution[0] > SteamDeckNativeResolution[0] || fullscreenResolution[1] > SteamDeckNativeResolution[1])
      setIfDifferent("fullscreenResolution", deckResolution);

    if (configuration->get("interfaceScale").optFloat().value(0) >= 1.75f)
      setIfDifferent("interfaceScale", 0);

    if (!configuration->get("controllerInput").optBool().value(false))
      setIfDifferent("controllerInput", true);

    if (!configuration->get("limitTextureAtlasSize").optBool().value(false))
      setIfDifferent("limitTextureAtlasSize", true);

    if (configuration->get("borderless").optBool().value(false) && configuration->get("maximized").optBool().value(false))
      setIfDifferent("maximized", false);

    configuration->set("steamDeckProfileVersion", SteamDeckProfileVersion);
    if (changed)
      Logger::info("Applied Steam Deck client profile");
  }
}

void ClientApplication::parseBenchmarkArgs(StringList& cmdLineArgs) {
  StringList filteredArgs;
  filteredArgs.reserve(cmdLineArgs.size());

  bool warmupDurationSet = false;
  bool areaPrimaryDurationSet = false;
  bool areaShipDurationSet = false;
  bool areaOrbitedDurationSet = false;
  bool assetSweepSet = false;

  for (auto const& argument : cmdLineArgs) {
    bool consumed = false;

    auto parseBoolOption = [&](char const* key, bool& output, bool defaultValue, bool emptyValue, bool* touched = nullptr) {
      if (auto value = parsePlusArgValue(argument, key)) {
        output = parseBoolLike(*value, defaultValue, emptyValue);
        consumed = true;
        if (touched)
          *touched = true;
      }
    };

    auto parseDoubleOption = [&](char const* key, double& output, double defaultValue, double minimum, double maximum, bool* touched = nullptr) {
      if (auto value = parsePlusArgValue(argument, key)) {
        output = parseDoubleLike(*value, defaultValue, minimum, maximum);
        consumed = true;
        if (touched)
          *touched = true;
      }
    };

    auto parseUIntOption = [&](char const* key, uint64_t& output, uint64_t defaultValue, uint64_t minimum, uint64_t maximum) {
      if (auto value = parsePlusArgValue(argument, key)) {
        output = parseUIntLike(*value, defaultValue, minimum, maximum);
        consumed = true;
      }
    };

    if (auto value = parsePlusArgValue(argument, "benchmark")) {
      m_benchmark.enabled = parseBoolLike(*value, true, true);
      consumed = true;
    } else if (auto value = parsePlusArgValue(argument, "gfxBenchmark")) {
      m_benchmark.enabled = parseBoolLike(*value, true, true);
      consumed = true;
    }

    parseBoolOption("benchmarkAutoQuit", m_benchmark.autoQuit, false, true);
    parseBoolOption("benchmarkCameraSweep", m_benchmark.forceCameraSweep, true, true);
    parseBoolOption("benchmarkAssetSweep", m_benchmark.assetScanEnabled, true, true, &assetSweepSet);
    parseBoolOption("benchmarkStress", m_benchmark.stressMode, false, true);
    parseBoolOption("benchmarkStressZoomOut", m_benchmark.stressForceZoomOut, true, true);
    parseBoolOption("benchmarkStressExtendedDurations", m_benchmark.stressUseExtendedDurations, true, true);
    parseBoolOption("benchmarkSampleReadyOnly", m_benchmark.sampleOnlyReadySectors, true, true);
    parseBoolOption("benchmarkIsolateStorage", m_benchmark.isolateStorage, true, true);
    parseBoolOption("benchmarkLateGameWorld", m_benchmark.requireLateGameWorld, true, true);
    parseBoolOption("benchmarkLateGameTerrestrial", m_benchmark.preferTerrestrialLateGameWorld, true, true);

    parseDoubleOption("benchmarkWarmup", m_benchmark.warmupSeconds, m_benchmark.warmupSeconds, 0.0, 300.0, &warmupDurationSet);
    parseDoubleOption("benchmarkDuration", m_benchmark.areaPrimarySeconds, m_benchmark.areaPrimarySeconds, 5.0, 1800.0, &areaPrimaryDurationSet);
    parseDoubleOption("benchmarkAreaDuration", m_benchmark.areaPrimarySeconds, m_benchmark.areaPrimarySeconds, 5.0, 1800.0, &areaPrimaryDurationSet);
    parseDoubleOption("benchmarkAreaPrimary", m_benchmark.areaPrimarySeconds, m_benchmark.areaPrimarySeconds, 5.0, 1800.0, &areaPrimaryDurationSet);
    parseDoubleOption("benchmarkShipDuration", m_benchmark.areaShipSeconds, m_benchmark.areaShipSeconds, 5.0, 1800.0, &areaShipDurationSet);
    parseDoubleOption("benchmarkAreaShip", m_benchmark.areaShipSeconds, m_benchmark.areaShipSeconds, 5.0, 1800.0, &areaShipDurationSet);
    parseDoubleOption("benchmarkOrbitedDuration", m_benchmark.areaOrbitedSeconds, m_benchmark.areaOrbitedSeconds, 5.0, 1800.0, &areaOrbitedDurationSet);
    parseDoubleOption("benchmarkAreaOrbited", m_benchmark.areaOrbitedSeconds, m_benchmark.areaOrbitedSeconds, 5.0, 1800.0, &areaOrbitedDurationSet);
    parseDoubleOption("benchmarkWarpTimeout", m_benchmark.warpTimeoutSeconds, m_benchmark.warpTimeoutSeconds, 5.0, 600.0);
    parseDoubleOption("benchmarkAreaSettle", m_benchmark.areaSettleSeconds, m_benchmark.areaSettleSeconds, 0.0, 30.0);
    parseDoubleOption("benchmarkAreaPrewarm", m_benchmark.areaPrewarmSweepSeconds, m_benchmark.areaPrewarmSweepSeconds, 0.0, 60.0);
    parseDoubleOption("benchmarkCameraAmplitude", m_benchmark.cameraAmplitudeTiles, m_benchmark.cameraAmplitudeTiles, 5.0, 500.0);
    parseDoubleOption("benchmarkCameraFrequency", m_benchmark.cameraFrequencyHz, m_benchmark.cameraFrequencyHz, 0.01, 5.0);
    parseDoubleOption("benchmarkStressAiInterval", m_benchmark.stressAiWaveIntervalSeconds, m_benchmark.stressAiWaveIntervalSeconds, 0.1, 60.0);
    parseDoubleOption("benchmarkStressItemInterval", m_benchmark.stressItemWaveIntervalSeconds, m_benchmark.stressItemWaveIntervalSeconds, 0.1, 60.0);
    parseDoubleOption("benchmarkStressTerrainInterval", m_benchmark.stressTerrainPulseIntervalSeconds, m_benchmark.stressTerrainPulseIntervalSeconds, 0.1, 60.0);
    parseDoubleOption("benchmarkStressLiquidInterval", m_benchmark.stressLiquidPulseIntervalSeconds, m_benchmark.stressLiquidPulseIntervalSeconds, 0.1, 60.0);
    parseDoubleOption("benchmarkStressExplosionInterval", m_benchmark.stressExplosionPulseIntervalSeconds, m_benchmark.stressExplosionPulseIntervalSeconds, 0.1, 60.0);
    parseDoubleOption("benchmarkStressJumpInterval", m_benchmark.stressJumpIntervalSeconds, m_benchmark.stressJumpIntervalSeconds, 0.1, 60.0);
    parseDoubleOption("benchmarkStressWeatherInterval", m_benchmark.stressWeatherPulseIntervalSeconds, m_benchmark.stressWeatherPulseIntervalSeconds, 1.0, 300.0);
    parseDoubleOption("benchmarkLateGameThreat", m_benchmark.lateGameThreatLevel, m_benchmark.lateGameThreatLevel, 0.1, 20.0);

    parseUIntOption("benchmarkAssetSample", m_benchmark.assetSampleCount, m_benchmark.assetSampleCount, 16, 200000);
    parseUIntOption("benchmarkSlowAssetCount", m_benchmark.maxSlowAssets, m_benchmark.maxSlowAssets, 4, 1024);
    parseUIntOption("benchmarkMinLoadedSectors", m_benchmark.minLoadedSectors, m_benchmark.minLoadedSectors, 0, 4096);
    parseUIntOption("benchmarkStressItems", m_benchmark.stressItemDrops, m_benchmark.stressItemDrops, 0, 10000);
    parseUIntOption("benchmarkStressNpcs", m_benchmark.stressNpcCount, m_benchmark.stressNpcCount, 0, 2000);
    parseUIntOption("benchmarkStressMonsters", m_benchmark.stressMonsterCount, m_benchmark.stressMonsterCount, 0, 4000);
    parseUIntOption("benchmarkStressLiquids", m_benchmark.stressLiquidBursts, m_benchmark.stressLiquidBursts, 0, 4000);

    if (auto value = parsePlusArgValue(argument, "benchmarkOutput")) {
      m_benchmark.outputPath = value->trim();
      consumed = true;
    }

    if (!consumed && (argument.beginsWith("+benchmark", String::CaseInsensitive) || argument.beginsWith("+gfxBenchmark", String::CaseInsensitive))) {
      consumed = true;
      Logger::warn("Benchmark: ignoring unrecognized argument '{}'", argument);
    }

    if (!consumed)
      filteredArgs.append(argument);
  }

  if (m_benchmark.stressMode && m_benchmark.stressUseExtendedDurations) {
    if (!warmupDurationSet)
      m_benchmark.warmupSeconds = 10.0;
    if (!areaPrimaryDurationSet)
      m_benchmark.areaPrimarySeconds = 140.0;
    if (!areaShipDurationSet)
      m_benchmark.areaShipSeconds = 75.0;
    if (!areaOrbitedDurationSet)
      m_benchmark.areaOrbitedSeconds = 65.0;
    if (!assetSweepSet)
      m_benchmark.assetScanEnabled = false;
  }

  cmdLineArgs = std::move(filteredArgs);
  m_benchmark.phase = m_benchmark.enabled ? BenchmarkPhase::WaitingForWorld : BenchmarkPhase::Disabled;
}

String ClientApplication::benchmarkPhaseName(BenchmarkPhase phase) const {
  switch (phase) {
    case BenchmarkPhase::Disabled:
      return "disabled";
    case BenchmarkPhase::WaitingForWorld:
      return "waiting_for_world";
    case BenchmarkPhase::Warmup:
      return "warmup";
    case BenchmarkPhase::AreaSweepPrimary:
      return "area_sweep_primary";
    case BenchmarkPhase::WarpToShip:
      return "warp_to_ship";
    case BenchmarkPhase::AreaSweepShip:
      return "area_sweep_ship";
    case BenchmarkPhase::WarpToOrbitedWorld:
      return "warp_to_orbited_world";
    case BenchmarkPhase::AreaSweepOrbitedWorld:
      return "area_sweep_orbited_world";
    case BenchmarkPhase::AssetLoadSweep:
      return "asset_load_sweep";
    case BenchmarkPhase::Finalize:
      return "finalize";
    case BenchmarkPhase::Completed:
      return "completed";
    default:
      return "unknown";
  }
}

void ClientApplication::benchmarkEnterPhase(BenchmarkPhase phase) {
  m_benchmark.phase = phase;
  m_benchmark.phaseStartedAtSeconds = monotonicSeconds();
  m_benchmark.areaSweepEnteredAtSeconds = m_benchmark.phaseStartedAtSeconds;
  m_benchmark.areaSweepReadyForSampling = false;
  m_benchmark.areaSweepReadySinceSeconds = 0.0;

  bool areaPhase = phase == BenchmarkPhase::AreaSweepPrimary
      || phase == BenchmarkPhase::AreaSweepShip
      || phase == BenchmarkPhase::AreaSweepOrbitedWorld;
  if (m_benchmark.stressMode && areaPhase)
    benchmarkResetStressActions();

  Logger::info("Benchmark phase -> {}", benchmarkPhaseName(phase));
}

bool ClientApplication::benchmarkPhaseElapsed(double seconds) const {
  return monotonicSeconds() >= m_benchmark.phaseStartedAtSeconds + seconds;
}

void ClientApplication::benchmarkConfigureIsolatedStorage(StringList& startupArgs) {
  if (!m_benchmark.enabled || !m_benchmark.isolateStorage)
    return;

  String sourceBootConfig = benchmarkSourceBootConfigPath(startupArgs);
  String sourceBootConfigPath = File::fullPath(sourceBootConfig);
  Json bootConfig = Json::parseJson(File::readFileString(sourceBootConfigPath));

  String benchmarkRoot = File::temporaryDirectory();
  String benchmarkStorage = File::relativeTo(benchmarkRoot, "storage");
  File::makeDirectoryRecursive(benchmarkStorage);

  bootConfig = bootConfig.set("storageDirectory", benchmarkStorage);
  bootConfig = bootConfig.set("logDirectory", File::relativeTo(benchmarkStorage, "logs"));

  String generatedBootConfig = File::relativeTo(benchmarkRoot, "sbinit.benchmark.config");
  File::writeFile(bootConfig.printJson(2, true), generatedBootConfig);

  StringList filteredArgs;
  filteredArgs.reserve(startupArgs.size() + 2);
  filteredArgs.append("-bootconfig");
  filteredArgs.append(generatedBootConfig);

  for (size_t i = 0; i < startupArgs.size(); ++i) {
    String const& arg = startupArgs.at(i);
    if (arg.equals("-bootconfig") || arg.equals("--bootconfig")) {
      if (i + 1 < startupArgs.size())
        ++i;
      continue;
    }

    if (arg.beginsWith("-bootconfig=", String::CaseInsensitive) || arg.beginsWith("--bootconfig=", String::CaseInsensitive))
      continue;

    filteredArgs.append(arg);
  }

  startupArgs = std::move(filteredArgs);
  m_benchmark.benchmarkStorageDirectory = benchmarkStorage;
  m_benchmark.benchmarkBootConfigPath = generatedBootConfig;

  Logger::info(
      "Benchmark: using isolated storage '{}' via generated boot config '{}'",
      m_benchmark.benchmarkStorageDirectory,
      m_benchmark.benchmarkBootConfigPath);
}

void ClientApplication::startup(StringList const& cmdLineArgs) {
  auto startupArgs = cmdLineArgs;
  parseBenchmarkArgs(startupArgs);
  benchmarkConfigureIsolatedStorage(startupArgs);

  RootLoader rootLoader({AdditionalAssetsSettings, AdditionalDefaultConfiguration, String("starbound.log"), LogLevel::Info, false, String("starbound.config")});
  m_root = rootLoader.initOrDie(startupArgs).first;

  if (m_benchmark.enabled) {
    m_root->configuration()->set("clearUniverseFiles", true);
    m_root->configuration()->set("clearPlayerFiles", true);

    if (m_benchmark.outputPath.empty()) {
      m_benchmark.outputPath = m_root->toStoragePath(
          strf("graphics-benchmark-{}.json", Time::millisecondsSinceEpoch()));
    } else if (!m_benchmark.outputPath.beginsWith("/")) {
      m_benchmark.outputPath = m_root->toStoragePath(m_benchmark.outputPath);
    }

    Logger::info(
        "Graphics benchmark enabled. Output='{}', storage='{}', warmup={}s, worldSweep={}s, shipSweep={}s, orbitedSweep={}s, areaPrewarm={}s, areaSettle={}s, minLoadedSectors={}, sampleReadyOnly={}, assetSweep={}, assetSampleLimit={}, stressMode={}, stressExtendedDurations={}, stressItems={}, stressNpcs={}, stressMonsters={}, stressLiquids={}, stressZoomOut={}, stressAiInterval={}s, stressTerrainInterval={}s, stressExplosionInterval={}s, lateGameWorld={}, lateGameThreat={} terrestrialOnly={}",
        m_benchmark.outputPath,
        m_benchmark.benchmarkStorageDirectory.empty() ? m_root->settings().storageDirectory : m_benchmark.benchmarkStorageDirectory,
        m_benchmark.warmupSeconds,
        m_benchmark.areaPrimarySeconds,
        m_benchmark.areaShipSeconds,
        m_benchmark.areaOrbitedSeconds,
        m_benchmark.areaPrewarmSweepSeconds,
        m_benchmark.areaSettleSeconds,
        m_benchmark.minLoadedSectors,
        m_benchmark.sampleOnlyReadySectors,
        m_benchmark.assetScanEnabled,
        m_benchmark.assetSampleCount,
        m_benchmark.stressMode,
        m_benchmark.stressUseExtendedDurations,
        m_benchmark.stressItemDrops,
        m_benchmark.stressNpcCount,
        m_benchmark.stressMonsterCount,
        m_benchmark.stressLiquidBursts,
        m_benchmark.stressForceZoomOut,
        m_benchmark.stressAiWaveIntervalSeconds,
        m_benchmark.stressTerrainPulseIntervalSeconds,
        m_benchmark.stressExplosionPulseIntervalSeconds,
        m_benchmark.requireLateGameWorld,
        m_benchmark.lateGameThreatLevel,
        m_benchmark.preferTerrestrialLateGameWorld);
  }

  Logger::info("OpenStarbound Client v{} for v{} ({}) Source ID: {} Protocol: {}", OpenStarVersionString, StarVersionString, StarArchitectureString, StarSourceIdentifierString, StarProtocolVersion);
}

void ClientApplication::shutdown() {
  // Clear HTTP trust request callback
  LuaBindings::clearHttpTrustRequestCallback();

  m_mainInterface.reset();

  if (m_universeClient)
    m_universeClient->disconnect();

  if (m_universeServer) {
    m_universeServer->stop();
    m_universeServer->join();
    m_universeServer.reset();
  }

  if (m_statistics) {
    m_statistics->writeStatistics();
    m_statistics.reset();
  }

  m_universeClient.reset();
  m_statistics.reset();
}

void ClientApplication::applicationInit(ApplicationControllerPtr appController) {
  Application::applicationInit(appController);

  appController->setCursorVisible(true);

  auto configuration = m_root->configuration();
  applySteamDeckProfile(configuration);

  bool vsync = configuration->get("vsync").toBool();
  Vec2U windowedSize = jsonToVec2U(configuration->get("windowedResolution"));
  Vec2U fullscreenSize = jsonToVec2U(configuration->get("fullscreenResolution"));
  bool fullscreen = configuration->get("fullscreen").toBool();
  bool borderless = configuration->get("borderless").toBool();
  bool maximized = configuration->get("maximized").toBool();
  m_controllerInput = configuration->get("controllerInput").optBool().value(false);
  
  #ifdef STAR_SYSTEM_WINDOWS
    appController->setBorderlessWorkaround(configuration->get("borderlessWorkaround", true).toBool());
  #endif

  if (fullscreen)
    appController->setFullscreenWindow(fullscreenSize);
  else if (borderless)
    appController->setBorderlessWindow();
  else if (maximized)
    appController->setMaximizedWindow();
  else
    appController->setNormalWindow(windowedSize);

  float updateRate = 1.0f / GlobalTimestep;
  if (auto jUpdateRate = configuration->get("updateRate")) {
    updateRate = jUpdateRate.toFloat();
    GlobalTimestep = 1.0f / updateRate;
  }

  if (auto jServerUpdateRate = configuration->get("serverUpdateRate"))
    ServerGlobalTimestep = 1.0f / jServerUpdateRate.toFloat();

  appController->setTargetUpdateRate(updateRate);
  appController->setVSyncEnabled(vsync);
  appController->setCursorHardware(configuration->get("hardwareCursor").optBool().value(true));

  // Must be called before anything that can invoke an asset load.
  loadMods();
  
  AudioFormat audioFormat = appController->enableAudio();
  m_mainMixer = make_shared<MainMixer>(audioFormat.sampleRate, audioFormat.channels);
  m_mainMixer->setVolume(0.5);
  
  m_worldPainter = make_shared<WorldPainter>();
  m_guiContext = make_shared<GuiContext>(m_mainMixer->mixer(), appController);
  m_input = make_shared<Input>();
  m_voice = make_shared<Voice>(appController);  

  auto assets = m_root->assets();

  {
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    m_immediateFont = *assets->bytes("/hobo.ttf");
    ImFontConfig config{};
    config.FontDataOwnedByAtlas = false;
    config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint;
    ImFont* immediateFont = io.Fonts->AddFontFromMemoryTTF(m_immediateFont.ptr(), m_immediateFont.size(),
      16, &config, io.Fonts->GetGlyphRangesDefault());
    if (!immediateFont) {
      Logger::warn("ImGui: failed to load '/hobo.ttf' for immediate UI font, using default font");
      immediateFont = io.Fonts->AddFontDefault();
    }

    io.FontDefault = immediateFont;
    if (!io.Fonts->IsBuilt())
      io.Fonts->Build();
  }

  m_minInterfaceScale = assets->json("/interface.config:minInterfaceScale").toFloat();
  m_maxInterfaceScale = assets->json("/interface.config:maxInterfaceScale").toFloat();
  m_crossoverRes = jsonToVec2F(assets->json("/interface.config:interfaceCrossoverRes"));
  
  appController->setApplicationTitle(assets->json("/client.config:windowTitle").toString());
  appController->setMaxFrameSkip(assets->json("/client.config:maxFrameSkip").toUInt());
  appController->setUpdateTrackWindow(assets->json("/client.config:updateTrackWindow").toFloat());
  
  if (auto jVoice = configuration->get("voice"))
    m_voice->loadJson(jVoice.toObject(), true);

  m_voice->init();
  m_voice->setLocalSpeaker(0);
}

void ClientApplication::renderInit(RendererPtr renderer) {
  Application::renderInit(renderer);
  renderReload();
  m_root->registerReloadListener(m_reloadListener = make_shared<CallbackListener>([this]() { renderReload(); }));

  if (m_root->configuration()->get("limitTextureAtlasSize").optBool().value(false))
    renderer->setSizeLimitEnabled(true);

  renderer->setMultiTexturingEnabled(m_root->configuration()->get("useMultiTexturing").optBool().value(true));

  m_guiContext->renderInit(renderer);
  refreshInterfaceScale();

  m_cinematicOverlay = make_shared<Cinematic>();
  m_errorScreen = make_shared<ErrorScreen>();

  if (m_titleScreen)
    m_titleScreen->renderInit(renderer);
  if (m_worldPainter)
    m_worldPainter->renderInit(renderer);

  #ifdef STAR_ENABLE_STEAM_INTEGRATION
  #ifdef STAR_SYSTEM_LINUX
  if (g_steamIsFlatpak) {
    auto config = m_root->configuration();
    if (!config->get("steamFlatpakWarningShown").optBool().value()) {
      config->set("steamFlatpakWarningShown", true);
      m_errorScreen->setMessage(m_root->assets()->json("/interface.config:steamFlatpakWarning").toString());
      changeState(MainAppState::SteamFlatpakWarning);
      return;
    }
  }
  #endif
  #endif

  changeState(MainAppState::Mods);
}

void ClientApplication::windowChanged(WindowMode windowMode, Vec2U screenSize) {
  auto config = m_root->configuration();
  if (windowMode == WindowMode::Fullscreen) {
    config->set("fullscreenResolution", jsonFromVec2U(screenSize));
    config->set("fullscreen", true);
    config->set("borderless", false);
  } else if (windowMode == WindowMode::Borderless) {
    config->set("borderless", true);
    config->set("fullscreen", false);
  } else if (windowMode == WindowMode::Maximized) {
    config->set("maximized", true);
    config->set("fullscreen", false);
    config->set("borderless", false);
  } else {
    config->set("maximized", false);
    config->set("fullscreen", false);
    config->set("borderless", false);
    config->set("windowedResolution", jsonFromVec2U(screenSize));
  }
}

void ClientApplication::processInput(InputEvent const& event) {
  if (auto keyDown = event.ptr<KeyDownEvent>()) {
    m_heldKeyEvents.append(*keyDown);
    m_edgeKeyEvents.append(*keyDown);
  } else if (auto keyUp = event.ptr<KeyUpEvent>()) {
    eraseWhere(m_heldKeyEvents, [&](auto& keyEvent) {
      return keyEvent.key == keyUp->key;
    });

    Maybe<KeyMod> modKey = KeyModNames.maybeLeft(KeyNames.getRight(keyUp->key));
    if (modKey)
      m_heldKeyEvents.transform([&](auto& keyEvent) {
        return KeyDownEvent{keyEvent.key, keyEvent.mods & ~*modKey};
      });
  }
  else if (auto cAxis = event.ptr<ControllerAxisEvent>()) {
    if (cAxis->controllerAxis == ControllerAxis::LeftX)
      m_controllerLeftStick[0] = cAxis->controllerAxisValue;
    else if (cAxis->controllerAxis == ControllerAxis::LeftY)
      m_controllerLeftStick[1] = cAxis->controllerAxisValue;
    else if (cAxis->controllerAxis == ControllerAxis::RightX)
      m_controllerRightStick[0] = cAxis->controllerAxisValue;
    else if (cAxis->controllerAxis == ControllerAxis::RightY)
      m_controllerRightStick[1] = cAxis->controllerAxisValue;
  }

  bool processed = !m_errorScreen->accepted() && m_errorScreen->handleInputEvent(event);

  if (!processed) {
    if (m_state == MainAppState::Splash) {
      processed = m_cinematicOverlay->handleInputEvent(event);
    } else if (m_state == MainAppState::Title) {
      if (!(processed = m_cinematicOverlay->handleInputEvent(event)))
        processed = m_titleScreen->handleInputEvent(event);

    } else if (m_state == MainAppState::SinglePlayer || m_state == MainAppState::MultiPlayer) {
      if (!(processed = m_cinematicOverlay->handleInputEvent(event)))
        processed = m_mainInterface->handleInputEvent(event);
    }
  }

  m_input->handleInput(event, processed);
}

void ClientApplication::update() {
  float dt = GlobalTimestep * GlobalTimescale;
  auto& app = appController();
  if (m_state >= MainAppState::Title) {
    if (auto p2pNetworkingService = app->p2pNetworkingService()) {
      if (auto join = p2pNetworkingService->pullPendingJoin()) {
        m_pendingMultiPlayerConnection = PendingMultiPlayerConnection{join.takeValue(), {}, {}, false};
        changeState(MainAppState::Title);
      }
      
      if (auto req = p2pNetworkingService->pullJoinRequest())
        m_mainInterface->queueJoinRequest(*req);

      p2pNetworkingService->update();
    }
  }

  if (!m_errorScreen->accepted())
    m_errorScreen->update(dt);

  // This warning is only applicable to Linux systems so no need to process it otherwise.
  #ifdef STAR_ENABLE_STEAM_INTEGRATION
  #ifdef STAR_SYSTEM_LINUX
  if (m_state == MainAppState::SteamFlatpakWarning)
    updateSteamFlatpakWarning(dt);
  else
  #endif
  #endif

  if (m_state == MainAppState::Mods)
    updateMods(dt);
  else if (m_state == MainAppState::ModsWarning)
    updateModsWarning(dt);

  if (m_state == MainAppState::Splash)
    updateSplash(dt);
  else if (m_state == MainAppState::Error)
    updateError(dt);
  else if (m_state == MainAppState::Title)
    updateTitle(dt);
  else if (m_state > MainAppState::Title)
    updateRunning(dt);
  
  // Swallow leftover encoded voice data if we aren't in-game to allow mic read to continue for settings.
  if (m_state <= MainAppState::Title) {
    DataStreamBuffer ext;
    m_voice->send(ext);
  } // TODO: directly disable encoding at menu so we don't have to do this

  m_guiContext->cleanup();
  m_edgeKeyEvents.clear();
  m_input->update();
  ++m_framesSkipped;
}

void ClientApplication::render() {
  uint64_t frameStartUs = Time::monotonicMicroseconds();
  uint64_t worldClientUs = 0;
  uint64_t worldPainterUs = 0;
  uint64_t worldTotalUs = 0;
  uint64_t interfaceUs = 0;

  m_framesSkipped = 0;
  auto config = m_root->configuration();
  auto assets = m_root->assets();
  auto& renderer = Application::renderer();

  if (isVulkanRendererId(renderer->rendererId()))
    renderer->setMultiSampling(0);
  else
    renderer->setMultiSampling(config->get("antiAliasing").optBool().value(false) ? 4 : 0);
  renderer->switchEffectConfig("interface");

  refreshInterfaceScale();

  if (m_state == MainAppState::Mods || m_state == MainAppState::Splash) {
    m_cinematicOverlay->render();

  } else if (m_state == MainAppState::Title) {
    m_titleScreen->render();
    m_cinematicOverlay->render();

  } else if (m_state > MainAppState::Title) {
    WorldClientPtr worldClient = m_universeClient->worldClient();
    if (worldClient) {
      auto totalStart = Time::monotonicMicroseconds();
      renderer->switchEffectConfig("world");
      auto clientStart = totalStart;
      worldClient->render(m_renderData, TilePainter::BorderTileSize);
      worldClientUs = Time::monotonicMicroseconds() - clientStart;
      LogMap::set("client_render_world_client", strf(u8"{:05d}\u00b5s", worldClientUs));

      auto paintStart = Time::monotonicMicroseconds();
      m_worldPainter->render(m_renderData, [&]() -> bool {
        return worldClient->waitForLighting(&m_renderData);
      });
      worldPainterUs = Time::monotonicMicroseconds() - paintStart;
      worldTotalUs = Time::monotonicMicroseconds() - totalStart;
      LogMap::set("client_render_world_painter", strf(u8"{:05d}\u00b5s", worldPainterUs));
      LogMap::set("client_render_world_total", strf(u8"{:05d}\u00b5s", worldTotalUs));
      
      auto size = Vec2F(renderer->screenSize());
      auto quad = renderFlatRect(RectF::withSize(size / -2, size), Vec4B::filled(0), 0.0f);
      for (auto& layer : m_postProcessLayers) {
        if (layer.group ? layer.group->enabled : true) {
          for (unsigned i = 0; i < layer.passes; i++) {
            for (auto& effect : layer.effects) {
              renderer->switchEffectConfig(effect);
              renderer->render(quad);
            }
          }
        }
      }
    }
    renderer->switchEffectConfig("interface");
    auto start = Time::monotonicMicroseconds();
    m_mainInterface->renderInWorldElements();
    m_mainInterface->render();
    m_cinematicOverlay->render();
    interfaceUs = Time::monotonicMicroseconds() - start;
    LogMap::set("client_render_interface", strf(u8"{:05d}\u00b5s", interfaceUs));
  }

  if (!m_errorScreen->accepted())
    m_errorScreen->render();

  if (m_benchmark.enabled && m_state > MainAppState::Title) {
    auto worldClient = m_universeClient ? m_universeClient->worldClient() : WorldClientPtr();
    benchmarkRecordFrameSample(
        worldClient,
        Time::monotonicMicroseconds() - frameStartUs,
        worldClientUs,
        worldPainterUs,
        worldTotalUs,
        interfaceUs);
  }
}

void ClientApplication::getAudioData(int16_t* sampleData, size_t frameCount) {
  if (m_mainMixer) {
    m_mainMixer->read(sampleData, frameCount, [&](int16_t* buffer, size_t frames, unsigned channels) {
      if (m_voice)
        m_voice->mix(buffer, frames, channels);
    });
  }
}

auto postProcessGroupsRoot = "postProcessGroups";

void ClientApplication::renderReload() {
  auto assets = m_root->assets();
  auto renderer = Application::renderer();

  auto loadEffectConfig = [&](String const& name) {
    String path = strf("/rendering/effects/{}.config", name);
    if (assets->assetExists(path)) {
      StringMap<String> shaders;
      auto config = assets->json(path);
      auto shaderConfig = config.getObject("effectShaders");
      for (auto& entry : shaderConfig) {
        if (entry.second.isType(Json::Type::String)) {
          String shader = entry.second.toString();
          if (!shader.hasChar('\n')) {
            auto shaderBytes = assets->bytes(AssetPath::relativeTo(path, shader));
            shader = std::string(shaderBytes->ptr(), shaderBytes->size());
          }
          shaders[entry.first] = shader;
        }
      }

      renderer->loadEffectConfig(name, config, shaders);
    } else
      Logger::warn("No rendering config found for renderer with id '{}'", renderer->rendererId());
  };

  String rendererConfigPath = "/rendering/opengl.config";
  auto rendererId = renderer->rendererId().toLower();
  if (rendererId.contains("vulkan"))
    rendererConfigPath = "/rendering/vulkan.config";
  else if (rendererId.contains("direct3d12"))
    rendererConfigPath = "/rendering/direct3d12.config";
  else if (rendererId.contains("metal"))
    rendererConfigPath = "/rendering/metal.config";

  if (!assets->assetExists(rendererConfigPath)) {
    if (rendererConfigPath != "/rendering/opengl.config")
      Logger::warn("No renderer config found at '{}', falling back to '/rendering/opengl.config'", rendererConfigPath);
    rendererConfigPath = "/rendering/opengl.config";
  }

  if (assets->assetExists(rendererConfigPath))
  {
    Json rendererConfig = assets->json(rendererConfigPath);
    if (isVulkanRendererId(renderer->rendererId()))
      rendererConfig = applyVulkanRendererConfigOverrides(std::move(rendererConfig), m_root->configuration());
    renderer->loadConfig(std::move(rendererConfig));
  }
  else
    Logger::warn("No renderer config found at '{}'", rendererConfigPath);
  
  loadEffectConfig("world");
  
  // define post process groups and set them to be enabled/disabled based on config
  
  auto config = m_root->configuration();
  if (!config->get(postProcessGroupsRoot).isType(Json::Type::Object))
    config->set(postProcessGroupsRoot, JsonObject());
  auto groupsConfig = config->get(postProcessGroupsRoot);
  
  m_postProcessGroups.clear();
  auto postProcessGroups = assets->json("/client.config:postProcessGroups").toObject();
  for (auto& pair : postProcessGroups) {
    auto name = pair.first;
    auto groupConfig = groupsConfig.opt(name);
    auto def = pair.second.getBool("enabledDefault",true);
    if (!groupConfig)
      config->setPath(strf("{}.{}", postProcessGroupsRoot, name),JsonObject());
    m_postProcessGroups.add(name,PostProcessGroup{ groupConfig ? groupConfig.value().getBool("enabled", def) : def });
  }
  
  // define post process layers and optionally assign them to groups
  m_postProcessLayers.clear();
  m_labelledPostProcessLayers.clear();
  auto postProcessLayers = assets->json("/client.config:postProcessLayers").toArray();
  for (auto& layer : postProcessLayers) {
    auto effects = jsonToStringList(layer.getArray("effects"));
    for (auto& effect : effects)
      loadEffectConfig(effect);
    PostProcessGroup* group = nullptr;
    auto gname = layer.optString("group");
    if (gname) {
      group = &m_postProcessGroups.get(gname.value());
    }
    // I'd think a string map for all of these would be better, but the order does matter here, and does make sense to depend on mod priority, so...
    // guess a string map of indices works
    // I tried pointers but for whatever reason the behaviour was highly inconsistent and only worked after reload...
    auto label = layer.optString("name");
    if (label) {
      m_labelledPostProcessLayers.add(label.value(),m_postProcessLayers.count());
    }
    m_postProcessLayers.append(PostProcessLayer{ std::move(effects), (unsigned)layer.getUInt("passes", 1), group });
  }

  loadEffectConfig("interface");
}

void ClientApplication::refreshInterfaceScale() {
  auto interfaceScale = m_root->configuration()->get("interfaceScale").optFloat().value();
  if (interfaceScale != 0) {
    m_guiContext->setInterfaceScale(interfaceScale);
  } else if (renderer() && m_guiContext->windowWidth() >= m_crossoverRes[0] && m_guiContext->windowHeight() >= m_crossoverRes[1]) {
    m_guiContext->setInterfaceScale(m_maxInterfaceScale);
  } else {
    m_guiContext->setInterfaceScale(m_minInterfaceScale);
  }
}

void ClientApplication::setPostProcessLayerPasses(String const& layer, unsigned const& passes) {
  m_postProcessLayers.at(m_labelledPostProcessLayers.get(layer)).passes = passes;
}
void ClientApplication::setPostProcessGroupEnabled(String const& group, bool const& enabled, Maybe<bool> const& save) {
  m_postProcessGroups.get(group).enabled = enabled;
  if (save && save.value())
    m_root->configuration()->setPath(strf("{}.{}.enabled", postProcessGroupsRoot, group),enabled);
}
bool ClientApplication::postProcessGroupEnabled(String const& group) {
  return m_postProcessGroups.get(group).enabled;
}

Json ClientApplication::postProcessGroups() {
  return m_root->assets()->json("/client.config:postProcessGroups");
}

unsigned ClientApplication::framesSkipped() const {
  return m_framesSkipped;
}

void ClientApplication::changeState(MainAppState newState) {
  MainAppState oldState = m_state;
  m_state = newState;
  auto& app = appController();

  if (m_state == MainAppState::Quit)
    app->quit();

  if (newState == MainAppState::Mods)
    m_cinematicOverlay->load(m_root->assets()->json("/cinematics/mods/modloading.cinematic"));

  if (newState == MainAppState::Splash) {
    m_cinematicOverlay->load(m_root->assets()->json("/cinematics/splash.cinematic"));
    m_rootLoader = Thread::invoke("Async root loader", [this]() {
        m_root->fullyLoad();
      });
  }

  if (oldState > MainAppState::Title && m_state <= MainAppState::Title) {
    if (m_universeClient)
      m_universeClient->disconnect();

    if (m_universeServer) {
      m_universeServer->stop();
      m_universeServer->join();
      m_universeServer.reset();
    }
    m_cinematicOverlay->stop();

    // Clear HTTP trust request callback
    LuaBindings::clearHttpTrustRequestCallback();

    m_mainInterface.reset();

    m_voice->clearSpeakers();

    if (auto p2pNetworkingService = app->p2pNetworkingService()) {
      p2pNetworkingService->setJoinUnavailable();
      p2pNetworkingService->setAcceptingP2PConnections(false);
    }
  }

  if (oldState > MainAppState::Title && m_state == MainAppState::Title) {
    m_titleScreen->resetState();
    m_mainMixer->setUniverseClient({});
  }
  if (oldState >= MainAppState::Title && m_state < MainAppState::Title) {
    m_playerStorage.reset();

    if (m_statistics) {
      m_statistics->writeStatistics();
      m_statistics.reset();
    }

    m_universeClient.reset();
    m_mainMixer->setUniverseClient({});
    m_titleScreen.reset();
  }

  if (oldState < MainAppState::Title && m_state >= MainAppState::Title) {
    if (m_rootLoader)
      m_rootLoader.finish();

    m_cinematicOverlay->stop();

    m_playerStorage = make_shared<PlayerStorage>(m_root->toStoragePath("player"));
    m_statistics = make_shared<Statistics>(m_root->toStoragePath("player"), app->statisticsService());
    m_universeClient = make_shared<UniverseClient>(m_playerStorage, m_statistics);

    m_universeClient->setLuaCallbacks("input", LuaBindings::makeInputCallbacks());
    m_universeClient->setLuaCallbacks("voice", LuaBindings::makeVoiceCallbacks());
    m_universeClient->setLuaCallbacks("camera", LuaBindings::makeCameraCallbacks(&m_worldPainter->camera()));
    m_universeClient->setLuaCallbacks("renderer", LuaBindings::makeRenderingCallbacks(this));

    Json alwaysAllow = m_root->configuration()->getPath("safe.alwaysAllowClipboard");
    m_universeClient->setLuaCallbacks("clipboard", LuaBindings::makeClipboardCallbacks(app, alwaysAllow && alwaysAllow.toBool()));
    const bool luaHttpEnabled = m_root->configuration()->getPath("safe.luaHttp.enabled").optBool().value(false);

    m_universeClient->setLuaCallbacks("http", LuaBindings::makeHttpCallbacks(luaHttpEnabled));

    auto heldScriptPanes = make_shared<List<MainInterface::ScriptPaneInfo>>();

    m_universeClient->playerReloadPreCallback() = [&, heldScriptPanes](bool resetInterface) {
      if (!resetInterface)
        return;

      m_mainInterface->takeScriptPanes(*heldScriptPanes);
    };

    m_universeClient->playerReloadCallback() = [&, heldScriptPanes](bool resetInterface) {
      auto paneManager = m_mainInterface->paneManager();
      if (auto inventory = paneManager->registeredPane<InventoryPane>(MainInterfacePanes::Inventory))
        inventory->clearChangedSlots();

      if (resetInterface) {
        m_mainInterface->reviveScriptPanes(*heldScriptPanes);
        heldScriptPanes->clear();
      }
    };

    m_mainMixer->setUniverseClient(m_universeClient);
    refreshInterfaceScale();
    m_titleScreen = make_shared<TitleScreen>(m_playerStorage, m_mainMixer->mixer(), m_universeClient);
    if (auto renderer = Application::renderer())
      m_titleScreen->renderInit(renderer);
  }

  if (m_state == MainAppState::Title) {
    auto configuration = m_root->configuration();

    if (m_pendingMultiPlayerConnection) {
      if (auto address = m_pendingMultiPlayerConnection->server.ptr<HostAddressWithPort>()) {
        m_titleScreen->setMultiPlayerAddress(toString(address->address()));
        m_titleScreen->setMultiPlayerPort(toString(address->port()));
        m_titleScreen->setMultiPlayerAccount(configuration->getPath("title.multiPlayerAccount").toString());
        m_titleScreen->setMultiPlayerForceLegacy(configuration->getPath("title.multiPlayerForceLegacy").optBool().value(false));
        m_titleScreen->goToMultiPlayerSelectCharacter(false);
      } else {
        m_titleScreen->goToMultiPlayerSelectCharacter(true);
      }
    } else {
      m_titleScreen->setMultiPlayerAddress(configuration->getPath("title.multiPlayerAddress").toString());
      m_titleScreen->setMultiPlayerPort(configuration->getPath("title.multiPlayerPort").toString());
      m_titleScreen->setMultiPlayerAccount(configuration->getPath("title.multiPlayerAccount").toString());
      m_titleScreen->setMultiPlayerForceLegacy(configuration->getPath("title.multiPlayerForceLegacy").optBool().value(false));
    }
  }

  if (m_state > MainAppState::Title) {
    m_player.reset();

    if (m_benchmark.enabled && m_benchmark.scenarioPlayerUuid) {
      m_player = m_playerStorage->loadPlayer(*m_benchmark.scenarioPlayerUuid);
      if (!m_player)
        m_benchmark.scenarioPlayerUuid = {};
    }

    if (!m_player && m_titleScreen->currentlySelectedPlayer()) {
      m_player = m_titleScreen->currentlySelectedPlayer();
    }

    if (!m_player) {
      if (auto uuid = m_playerStorage->playerUuidAt(0))
        m_player = m_playerStorage->loadPlayer(*uuid);
    }

    if (!m_player) {
      setError("Error loading player!");
      return;
    }

    if (m_benchmark.enabled)
      benchmarkConfigureScenarioPlayer(m_player);

    m_mainMixer->setUniverseClient(m_universeClient);
    m_universeClient->setMainPlayer(m_player);
    m_cinematicOverlay->setPlayer(m_player);
    m_timeSinceJoin = (int64_t)Time::millisecondsSinceEpoch() / 1000;

    auto assets = m_root->assets();
    String loadingCinematic = assets->json("/client.config:loadingCinematic").toString();
    m_cinematicOverlay->load(assets->json(loadingCinematic));
    if (!m_player->log()->introComplete()) {
      String introCinematic = assets->json("/client.config:introCinematic").toString();
      introCinematic = introCinematic.replaceTags(StringMap<String>{{"species", m_player->species()}});
      m_player->setPendingCinematic(Json(introCinematic));
    } else {
      m_player->setPendingCinematic(Json());
    }

    if (m_state == MainAppState::MultiPlayer) {
      PacketSocketUPtr packetSocket;

      auto multiPlayerConnection = m_pendingMultiPlayerConnection.take();

      if (auto address = multiPlayerConnection.server.ptr<HostAddressWithPort>()) {
        try {
          packetSocket = TcpPacketSocket::open(TcpSocket::connectTo(*address));
        } catch (StarException const& e) {
          setError(strf("Join failed! Error connecting to '{}'", *address), e);
          return;
        }

      } else {
        auto p2pPeerId = multiPlayerConnection.server.ptr<P2PNetworkingPeerId>();

        if (auto p2pNetworkingService = app->p2pNetworkingService()) {
          auto result = p2pNetworkingService->connectToPeer(*p2pPeerId);
          if (result.isLeft()) {
            setError(strf("Cannot join peer: {}", result.left()));
            return;
          } else {
            packetSocket = P2PPacketSocket::open(std::move(result.right()));
          }
        } else {
          setError("Internal error, no p2p networking service when joining p2p networking peer");
          return;
        }
      }

      bool allowAssetsMismatch = m_root->configuration()->get("allowAssetsMismatch").toBool();
      if (auto errorMessage = m_universeClient->connect(UniverseConnection(std::move(packetSocket)), allowAssetsMismatch,
            multiPlayerConnection.account, multiPlayerConnection.password, multiPlayerConnection.forceLegacy)) {
        setError(*errorMessage);
        return;
      }

      if (auto address = multiPlayerConnection.server.ptr<HostAddressWithPort>())
        m_currentRemoteJoin = *address;
      else
        m_currentRemoteJoin.reset();

    } else {
      if (!m_universeServer) {
        try {
          m_universeServer = make_shared<UniverseServer>(m_root->toStoragePath("universe"));
          m_universeServer->start();
        } catch (StarException const& e) {
          setError("Unable to start local server", e);
          return;
        }
      }

      if (auto errorMessage = m_universeClient->connect(m_universeServer->addLocalClient(), "", "")) {
        setError(strf("Error connecting locally: {}", *errorMessage));
        return;
      }
    }

    m_titleScreen->stopMusic();

    m_universeClient->restartLua();
    refreshInterfaceScale();
    m_mainInterface = make_shared<MainInterface>(m_universeClient, m_worldPainter, m_cinematicOverlay);
    m_universeClient->setLuaCallbacks("interface", LuaBindings::makeInterfaceCallbacks(m_mainInterface.get()));
    m_universeClient->setLuaCallbacks("chat", LuaBindings::makeChatCallbacks(m_mainInterface.get(), m_universeClient.get()));
    m_universeClient->setLuaCallbacks("celestial", LuaBindings::makeCelestialCallbacks(m_universeClient.get()));
    m_universeClient->setLuaCallbacks("team", LuaBindings::makeTeamClientCallbacks(m_universeClient->teamClient().get()));
    m_universeClient->setLuaCallbacks("world", LuaBindings::makeWorldCallbacks(m_universeClient->worldClient().get()));

    LuaBindings::setHttpTrustRequestCallback([mainInterface = m_mainInterface.get()](String const& domain) {
      const auto paneManager = mainInterface->paneManager();
      const auto httpTrustDialog = paneManager->registeredPane<HttpTrustDialog>(MainInterfacePanes::HttpTrustDialog);

      httpTrustDialog->displayRequest(domain, [domain](const HttpTrustReply reply, bool remember) {
        const bool allowed = (reply == HttpTrustReply::Allow);
        LuaBindings::handleHttpTrustReply(domain, allowed);
      });
      paneManager->displayRegisteredPane(MainInterfacePanes::HttpTrustDialog);
    });


    m_mainInterface->displayDefaultPanes();
    m_universeClient->startLuaScripts();

    m_mainMixer->setWorldPainter(m_worldPainter);

    if (auto renderer = Application::renderer()) {
      m_worldPainter->renderInit(renderer);
    }
  }
}

void ClientApplication::setError(String const& error) {
  Logger::error(error.utf8Ptr());
  m_errorScreen->setMessage(error);
  m_titleScreen->resetState();
  changeState(MainAppState::Title);
}

void ClientApplication::setError(String const& error, std::exception const& e) {
  Logger::error("{}\n{}", error, outputException(e, true));
  m_errorScreen->setMessage(strf("{}\n{}", error, outputException(e, false)));
  m_titleScreen->resetState();
  changeState(MainAppState::Title);
}

bool ClientApplication::benchmarkEnsureScenarioPlayer() {
  if (!m_benchmark.enabled || !m_playerStorage || !m_root)
    return false;

  // Always create a fresh benchmark pilot in isolated storage so each run starts
  // from a new player and ship state.
  PlayerPtr benchmarkPlayer = m_root->playerFactory()->create();
  if (benchmarkPlayer)
    benchmarkPlayer->setName(strf("BenchmarkPilot{}", Time::millisecondsSinceEpoch() % 1000000));
  if (!benchmarkPlayer)
    return false;

  benchmarkConfigureScenarioPlayer(benchmarkPlayer);
  m_playerStorage->savePlayer(benchmarkPlayer);
  m_playerStorage->moveToFront(benchmarkPlayer->uuid());
  m_benchmark.scenarioPlayerUuid = benchmarkPlayer->uuid();
  m_benchmark.scenarioPlayerReady = true;

  Logger::info(
      "Benchmark: prepared isolated scenario player '{}' ({})",
      Text::stripEscapeCodes(benchmarkPlayer->name()),
      benchmarkPlayer->uuid().hex());
  return true;
}

void ClientApplication::benchmarkConfigureScenarioPlayer(PlayerPtr const& player) {
  if (!player || !m_root)
    return;

  player->log()->setIntroComplete(true);
  player->setModeType(PlayerMode::Casual);

  String shipSpecies = player->shipSpecies();
  if (shipSpecies.empty())
    shipSpecies = player->species();
  if (shipSpecies.empty())
    shipSpecies = "human";
  player->setShipSpecies(shipSpecies);

  unsigned maxVanillaShipLevel = player->shipUpgrades().shipLevel;
  try {
    Json speciesShips = m_root->assets()->json("/universe_server.config:speciesShips");
    Maybe<Json> shipSpeciesList = speciesShips.opt(shipSpecies);
    if (!shipSpeciesList && !shipSpecies.equalsIgnoreCase("human"))
      shipSpeciesList = speciesShips.opt("human");

    if (shipSpeciesList && shipSpeciesList->isType(Json::Type::Array) && !shipSpeciesList->toArray().empty())
      maxVanillaShipLevel = std::max<unsigned>(maxVanillaShipLevel, (unsigned)shipSpeciesList->toArray().size() - 1);
  } catch (std::exception const& e) {
    m_benchmark.warnings.append(strf("Failed to read species ship list for benchmark setup: {}", outputException(e, false)));
  }

  ShipUpgrades configuredUpgrades = player->shipUpgrades();
  try {
    configuredUpgrades.apply(m_root->assets()->json("/ships/shipupgrades.config"));
  } catch (std::exception const& e) {
    m_benchmark.warnings.append(strf("Failed to read /ships/shipupgrades.config: {}", outputException(e, false)));
  }
  configuredUpgrades.shipLevel = std::max(configuredUpgrades.shipLevel, maxVanillaShipLevel);
  player->setShipUpgrades(configuredUpgrades);
}

Maybe<CelestialCoordinate> ClientApplication::benchmarkFindLateGameWorld() {
  if (!m_universeServer)
    return {};

  auto* masterDatabase = dynamic_cast<CelestialMasterDatabase*>(&m_universeServer->celestialDatabase());
  if (!masterDatabase)
    return {};

  struct SearchPass {
    float minThreat;
    bool terrestrialOnly;
    unsigned tries;
    unsigned range;
  };

  List<SearchPass> searchPasses{
      {(float)m_benchmark.lateGameThreatLevel, m_benchmark.preferTerrestrialLateGameWorld, 2200, 120},
      {(float)m_benchmark.lateGameThreatLevel, false, 2200, 120},
      {(float)std::max(4.5, m_benchmark.lateGameThreatLevel - 1.0), m_benchmark.preferTerrestrialLateGameWorld, 1800, 100},
      {(float)std::max(3.0, m_benchmark.lateGameThreatLevel - 2.0), false, 1400, 80},
  };

  for (auto const& pass : searchPasses) {
    auto candidate = masterDatabase->findRandomWorld(pass.tries, pass.range, [masterDatabase, pass](CelestialCoordinate const& coordinate) {
      auto parameters = masterDatabase->parameters(coordinate);
      if (!parameters)
        return false;

      auto visitableParameters = parameters->visitableParameters();
      if (!visitableParameters)
        return false;

      if (visitableParameters->threatLevel < pass.minThreat)
        return false;

      if (pass.terrestrialOnly && !dynamic_cast<TerrestrialWorldParameters const*>(visitableParameters.get()))
        return false;

      return true;
    });

    if (candidate)
      return candidate;
  }

  return {};
}

bool ClientApplication::benchmarkEnsureLateGameWorld(WorldClientPtr const& worldClient, String const& worldId) {
  if (!m_benchmark.enabled || !m_benchmark.requireLateGameWorld)
    return true;

  bool inCelestialWorld = worldId.beginsWith("CelestialWorld:", String::CaseInsensitive);
  if (inCelestialWorld && worldClient && worldClient->threatLevel() >= (float)m_benchmark.lateGameThreatLevel) {
    m_benchmark.scenarioLateGameReady = true;
    m_benchmark.scenarioPreparationAttempts = 0;
    return true;
  }

  m_benchmark.scenarioLateGameReady = false;

  if (!m_universeServer || !m_universeClient || !m_universeClient->clientContext())
    return false;

  ConnectionId connectionId = m_universeClient->clientContext()->connectionId();
  if (connectionId == 0)
    return false;

  if (m_benchmark.stressMode && m_player) {
    m_universeServer->setAdmin(connectionId, true);
    m_player->setAdmin(true);
    if (m_player->isDead())
      m_player->revive(m_player->position() + m_player->feetOffset());
  }

  double now = monotonicSeconds();
  if (now < m_benchmark.scenarioNextActionAtSeconds)
    return false;

  if (!m_benchmark.lateGameTargetCoordinate) {
    m_benchmark.lateGameTargetCoordinate = benchmarkFindLateGameWorld();
    if (!m_benchmark.lateGameTargetCoordinate) {
      m_benchmark.warnings.append(strf("Benchmark could not find a late-game world with threat >= {}", m_benchmark.lateGameThreatLevel));
      if (++m_benchmark.scenarioPreparationAttempts >= 8) {
        benchmarkFinalize("late_game_world_not_found");
        benchmarkEnterPhase(BenchmarkPhase::Completed);
      }
      m_benchmark.scenarioNextActionAtSeconds = now + 3.0;
      return false;
    }

    m_benchmark.lateGameTargetWorldId = printWorldId(CelestialWorldId(*m_benchmark.lateGameTargetCoordinate));
    m_benchmark.scenarioPreparationAttempts = 0;
    Logger::info(
        "Benchmark: selected late-game target world '{}' (minimum threat {})",
        m_benchmark.lateGameTargetWorldId,
        m_benchmark.lateGameThreatLevel);
  }

  CelestialCoordinate const& target = *m_benchmark.lateGameTargetCoordinate;
  if (m_universeServer->clientShipCoordinate(connectionId) != target) {
    m_universeServer->clientFlyShip(connectionId, target.location(), target);
    m_benchmark.scenarioNextActionAtSeconds = now + 2.5;
    Logger::info("Benchmark: flying ship toward late-game world {}", target);
    return false;
  }

  if (worldId.equalsIgnoreCase(m_benchmark.lateGameTargetWorldId) && inCelestialWorld && worldClient)
    return worldClient->threatLevel() >= (float)m_benchmark.lateGameThreatLevel;

  if (worldId.beginsWith("ClientShipWorld:", String::CaseInsensitive)) {
    m_universeClient->warpPlayer(WarpAlias::OrbitedWorld, false);
    m_benchmark.scenarioNextActionAtSeconds = now + 2.0;
    Logger::info("Benchmark: warping from ship to late-game target world '{}'", m_benchmark.lateGameTargetWorldId);
    return false;
  }

  m_universeClient->warpPlayer(WarpAlias::OwnShip, false);
  m_benchmark.scenarioNextActionAtSeconds = now + 2.0;
  Logger::info("Benchmark: returning to ship before late-game world warp");
  return false;
}

void ClientApplication::loadMods() {
  auto ugcService = appController()->userGeneratedContentService();
  auto configuration = m_root->configuration();
  bool includeUGC = configuration->get("includeUGC", m_root->settings().includeUGC).toBool();
  if (ugcService && includeUGC) {
    StringList modDirectories;
    Logger::info("Checking for user generated content...");
    for (auto& contentId : ugcService->subscribedContentIds()) {
      if (auto contentDirectory = ugcService->contentDownloadDirectory(contentId)) {
        Logger::info("Loading mods from user generated content with id '{}' from directory '{}'", contentId, *contentDirectory);
        modDirectories.append(*contentDirectory);
      } else {
        Logger::warn("User generated content with id '{}' is not available", contentId);
      }
    }

    if (modDirectories.empty()) {
      Logger::info("No subscribed user generated content");
    } else {
      Root::singleton().loadMods(modDirectories, false);
      auto assets = m_root->assets();
    }
  }
}

void ClientApplication::updateSteamFlatpakWarning(float) {
  if (m_errorScreen->accepted())
    changeState(MainAppState::Mods);
}

void ClientApplication::updateMods(float dt) {
  m_cinematicOverlay->update(dt);
  auto ugcService = appController()->userGeneratedContentService();
  auto configuration = m_root->configuration();
  bool includeUGC = configuration->get("includeUGC", m_root->settings().includeUGC).toBool();
  if (ugcService && includeUGC) {
    // Prevent unnecessary log spam when UGC needs to be downloaded
    if (!m_loggedUGCCheck) {
      Logger::info("Checking for user generated content updates...");
      m_loggedUGCCheck = true;
    }
    
    if (ugcService->triggerContentDownload() == UserGeneratedContentService::UGCState::NoDownload) {
      changeState(MainAppState::Splash);
    } else {
      if (ugcService->triggerContentDownload() == UserGeneratedContentService::UGCState::Finished) {
        Logger::info("Loading updated user generated content...");
        StringList modDirectories;
        for (auto& contentId : ugcService->subscribedContentIds()) {
          if (auto contentDirectory = ugcService->contentDownloadDirectory(contentId)) {
            Logger::info("Loading mods from user generated content with id '{}' from directory '{}'", contentId, *contentDirectory);
            modDirectories.append(*contentDirectory);
          } else {
            Logger::warn("User generated content with id '{}' is not available", contentId);
          }
        }

        if (modDirectories.empty()) {
          changeState(MainAppState::Splash);
        } else {
          Logger::info("Reloading to include updated user generated content");
          Root::singleton().loadMods(modDirectories);

          // We've just reloaded, so make sure to grab our config again!
          // If we don't do this, we'll be able to read modsWarningShown
          // just fine, but we won't be able to write it back to the file.
          configuration = m_root->configuration();
        }

        auto assets = m_root->assets();

        if (configuration->get("modsWarningShown").optBool().value()) {
          changeState(MainAppState::Splash);
        } else {
          configuration->set("modsWarningShown", true);
          m_errorScreen->setMessage(assets->json("/interface.config:modsWarningMessage").toString());
          changeState(MainAppState::ModsWarning);
        }
      }
    }
  } else {
    changeState(MainAppState::Splash);
  }
}

void ClientApplication::updateModsWarning(float) {
  if (m_errorScreen->accepted())
    changeState(MainAppState::Splash);
}

void ClientApplication::updateSplash(float dt) {
  m_cinematicOverlay->update(dt);
  if (m_benchmark.enabled && !m_rootLoader.isRunning()) {
    changeState(MainAppState::Title);
    return;
  }
  if (!m_rootLoader.isRunning() && (m_cinematicOverlay->completable() || m_cinematicOverlay->completed()))
    changeState(MainAppState::Title);
}

void ClientApplication::updateError(float) {
  if (m_errorScreen->accepted())
    changeState(MainAppState::Title);
}

void ClientApplication::updateTitle(float dt) {
  m_cinematicOverlay->update(dt);

  m_titleScreen->update(dt);
  m_mainMixer->update(dt);
  m_mainMixer->setSpeed(GlobalTimescale);

  auto& app = appController();
  bool inputActive = m_titleScreen->textInputActive();
  m_input->setTextInputActive(inputActive);
  if (inputActive)
    app->setTextArea(m_titleScreen->paneManager()->keyboardCapturedWidget()->keyboardCaptureArea());
  else
    app->setTextArea();
  app->setAcceptingTextInput(inputActive);

  auto p2pNetworkingService = app->p2pNetworkingService();
  if (p2pNetworkingService) {
    auto getStateString = [](TitleState state) -> const char* {
      switch (state) {
        case TitleState::Main:
          return "In Main Menu";
        case TitleState::Options:
          return "In Options";
        case TitleState::Mods:
          return "In Mods";
        case TitleState::SinglePlayerSelectCharacter:
          return "Selecting a character for singleplayer";
        case TitleState::SinglePlayerCreateCharacter:
          return "Creating a character for singleplayer";
        case TitleState::MultiPlayerSelectCharacter:
          return "Selecting a character for multiplayer";
        case TitleState::MultiPlayerCreateCharacter:
          return "Creating a character for multiplayer";
        case TitleState::MultiPlayerConnect:
          return "Awaiting multiplayer connection info";
        case TitleState::StartSinglePlayer:
          return "Loading Singleplayer";
        case TitleState::StartMultiPlayer:
          return "Connecting to Multiplayer";
        default:
          return "";
      }
    };

    p2pNetworkingService->setActivityData("Not In Game", getStateString(m_titleScreen->currentState()), 0, {});
  }

  // Benchmark mode is intended to be unattended when requested from CLI.
  // Auto-enter singleplayer with an isolated benchmark save to avoid blocking
  // on title / character selection UI and to keep benchmark data out of user
  // storage.
  if (m_benchmark.enabled && !m_benchmark.running && !m_benchmark.completed) {
    if (!benchmarkEnsureScenarioPlayer()) {
      setError("Graphics benchmark could not prepare isolated benchmark player");
      return;
    }

    Logger::info("Benchmark: auto-starting singleplayer with isolated benchmark player");
    changeState(MainAppState::SinglePlayer);
    return;
  }

  if (m_titleScreen->currentState() == TitleState::StartSinglePlayer) {
    changeState(MainAppState::SinglePlayer);

  } else if (m_titleScreen->currentState() == TitleState::StartMultiPlayer) {
    if (!m_pendingMultiPlayerConnection || m_pendingMultiPlayerConnection->server.is<HostAddressWithPort>()) {
      auto addressString = m_titleScreen->multiPlayerAddress().trim();
      auto portString = m_titleScreen->multiPlayerPort().trim();
      portString = portString.empty() ? toString(m_root->configuration()->get("gameServerPort").toUInt()) : portString;
      if (auto port = maybeLexicalCast<uint16_t>(portString)) {
        auto address = HostAddressWithPort::lookup(addressString, *port);
        if (address.isLeft()) {
          setError(address.left());
        } else {
          m_pendingMultiPlayerConnection = PendingMultiPlayerConnection{
            address.right(),
            m_titleScreen->multiPlayerAccount(),
            m_titleScreen->multiPlayerPassword(),
            m_titleScreen->multiPlayerForceLegacy()
          };

          auto configuration = m_root->configuration();
          configuration->setPath("title.multiPlayerAddress", m_titleScreen->multiPlayerAddress());
          configuration->setPath("title.multiPlayerPort", m_titleScreen->multiPlayerPort());
          configuration->setPath("title.multiPlayerAccount", m_titleScreen->multiPlayerAccount());
          configuration->setPath("title.multiPlayerForceLegacy", m_titleScreen->multiPlayerForceLegacy());

          changeState(MainAppState::MultiPlayer);
        }
      } else {
        setError(strf("invalid port: {}", portString));
      }
    } else {
      changeState(MainAppState::MultiPlayer);
    }

  } else if (m_titleScreen->currentState() == TitleState::Quit) {
    changeState(MainAppState::Quit);
  }
}

void ClientApplication::updateRunning(float dt) {
  try {
    auto& app = appController();
    auto worldClient = m_universeClient->worldClient();
    auto p2pNetworkingService = app->p2pNetworkingService();
    bool clientIPJoinable = m_root->configuration()->get("clientIPJoinable").toBool();
    bool clientP2PJoinable = m_root->configuration()->get("clientP2PJoinable").toBool();
    Maybe<pair<uint16_t, uint16_t>> party = make_pair(m_universeClient->players(), m_universeClient->maxPlayers());

    if (m_state == MainAppState::MultiPlayer) {
      if (p2pNetworkingService) {
        p2pNetworkingService->setAcceptingP2PConnections(false);
        if (clientP2PJoinable && m_currentRemoteJoin)
          p2pNetworkingService->setJoinRemote(*m_currentRemoteJoin);
        else
          p2pNetworkingService->setJoinUnavailable();
      }
    } else {
      m_universeServer->setListeningTcp(clientIPJoinable);
      if (p2pNetworkingService) {
        p2pNetworkingService->setAcceptingP2PConnections(clientP2PJoinable);
        if (clientP2PJoinable) {
          p2pNetworkingService->setJoinLocal(m_universeServer->maxClients());
        } else {
          p2pNetworkingService->setJoinUnavailable();
          party = {};
        }
      }
    }
    
    if (p2pNetworkingService) {
      auto getActivityDetail = [&](String const& tag) -> String {
        if (tag == "playerName")
          return Text::stripEscapeCodes(m_player->name());
        if (tag == "playerHealth")
          return toString(m_player->health());
        if (tag == "playerMaxHealth")
          return toString(m_player->maxHealth());
        if (tag == "playerEnergy")
          return toString(m_player->energy());
        if (tag == "playerMaxEnergy")
          return toString(m_player->maxEnergy());
        if (tag == "playerBreath")
          return toString(m_player->breath());
        if (tag == "playerMaxBreath")
          return toString(m_player->maxBreath());
        if (tag == "playerXPos")
          return toString(round(m_player->position().x()));
        if (tag == "playerYPos")
          return toString(round(m_player->position().y()));
        if (tag == "worldName") {
          if (m_universeClient->clientContext()->playerWorldId().is<ClientShipWorldId>())
            return "Player Ship";
          else if (WorldTemplate const* worldTemplate = worldClient ? worldClient->currentTemplate().get() : nullptr) {
            auto worldName = worldTemplate->worldName();
            if (worldName.empty())
              return "In World";
            else
              return Text::stripEscapeCodes(worldName);
          }
          else
            return "Nowhere";
        }
        return "";
      };

      String finalDetails = "";
      Json activityDetails = m_root->configuration()->getPath("discord.activityDetails");
      if (activityDetails.isType(Json::Type::Array)) {
        StringList detailsList;
        for (auto& detail : activityDetails.iterateArray())
          detailsList.append(getActivityDetail(*detail.stringPtr()));
        finalDetails = detailsList.join("\n");
      } else if (activityDetails.isType(Json::Type::String))
        finalDetails = activityDetails.toString().lookupTags(getActivityDetail);

      p2pNetworkingService->setActivityData("In Game", finalDetails.utf8Ptr(), m_timeSinceJoin, party);
    }

    if (!m_mainInterface->inputFocus() && !m_cinematicOverlay->suppressInput()) {
      m_player->setShifting(isActionTaken(InterfaceAction::PlayerShifting));

      if (isActionTaken(InterfaceAction::PlayerRight))
        m_player->moveRight();
      if (isActionTaken(InterfaceAction::PlayerLeft))
        m_player->moveLeft();
      if (isActionTaken(InterfaceAction::PlayerUp))
        m_player->moveUp();
      if (isActionTaken(InterfaceAction::PlayerDown))
        m_player->moveDown();
      if (isActionTaken(InterfaceAction::PlayerJump))
        m_player->jump();

      if (isActionTaken(InterfaceAction::PlayerTechAction1))
        m_player->special(1);
      if (isActionTaken(InterfaceAction::PlayerTechAction2))
        m_player->special(2);
      if (isActionTaken(InterfaceAction::PlayerTechAction3))
        m_player->special(3);

      if (isActionTakenEdge(InterfaceAction::PlayerInteract))
        m_player->beginTrigger();
      else if (!isActionTaken(InterfaceAction::PlayerInteract))
        m_player->endTrigger();

      if (isActionTakenEdge(InterfaceAction::PlayerDropItem))
        m_player->dropItem();

      if (isActionTakenEdge(InterfaceAction::EmoteBlabbering))
        m_player->addEmote(HumanoidEmote::Blabbering);
      if (isActionTakenEdge(InterfaceAction::EmoteShouting))
        m_player->addEmote(HumanoidEmote::Shouting);
      if (isActionTakenEdge(InterfaceAction::EmoteHappy))
        m_player->addEmote(HumanoidEmote::Happy);
      if (isActionTakenEdge(InterfaceAction::EmoteSad))
        m_player->addEmote(HumanoidEmote::Sad);
      if (isActionTakenEdge(InterfaceAction::EmoteNeutral))
        m_player->addEmote(HumanoidEmote::NEUTRAL);
      if (isActionTakenEdge(InterfaceAction::EmoteLaugh))
        m_player->addEmote(HumanoidEmote::Laugh);
      if (isActionTakenEdge(InterfaceAction::EmoteAnnoyed))
        m_player->addEmote(HumanoidEmote::Annoyed);
      if (isActionTakenEdge(InterfaceAction::EmoteOh))
        m_player->addEmote(HumanoidEmote::Oh);
      if (isActionTakenEdge(InterfaceAction::EmoteOooh))
        m_player->addEmote(HumanoidEmote::OOOH);
      if (isActionTakenEdge(InterfaceAction::EmoteBlink))
        m_player->addEmote(HumanoidEmote::Blink);
      if (isActionTakenEdge(InterfaceAction::EmoteWink))
        m_player->addEmote(HumanoidEmote::Wink);
      if (isActionTakenEdge(InterfaceAction::EmoteEat))
        m_player->addEmote(HumanoidEmote::Eat);
      if (isActionTakenEdge(InterfaceAction::EmoteSleep))
        m_player->addEmote(HumanoidEmote::Sleep);

      if (int newZoomDirection = (int)m_input->bindHeld("opensb", "zoomIn") - (int)m_input->bindHeld("opensb", "zoomOut"))
        m_cameraZoomDirection = newZoomDirection;
    }
    if (m_cameraZoomDirection != 0) {
      const float threshold = 0.01f;
      bool goingIn = m_cameraZoomDirection == 1;
      auto config = m_root->configuration();
      float curZoom = config->get("zoomLevel").toFloat(),
            newZoom = max(1.f, curZoom * powf(1.f + (float)m_cameraZoomDirection * 0.5f, min(1.f, dt * 5.f))),
            intZoom = max(1.f, (goingIn ? floor(curZoom) : ceil(curZoom)) + m_cameraZoomDirection);
      bool pastInt = goingIn ? newZoom + threshold > intZoom
                             : newZoom - threshold < intZoom;
      if (pastInt) {
        float intNewZoom = goingIn ? ceil(newZoom) : floor(newZoom);
        newZoom = lerp(clamp(abs(intZoom - newZoom) - 1.f, 0.f, 1.f), intZoom, intNewZoom);
        m_cameraZoomDirection = 0;
      }
      config->set("zoomLevel", min(1000000.f, newZoom));
    }

    if (m_controllerInput && m_controllerLeftStick.magnitudeSquared() > 0.01f)
      m_player->setMoveVector(m_controllerLeftStick);
    else
      m_player->setMoveVector(Vec2F());

    m_voice->setInput(m_input->bindHeld("opensb", "pushToTalk"));
    DataStreamBuffer voiceData;
    voiceData.setByteOrder(ByteOrder::LittleEndian);
    //voiceData.writeBytes(VoiceBroadcastPrefix.utf8Bytes()); transmitting with SE compat for now
    bool needstoSendVoice = m_voice->send(voiceData, 5000);

    auto checkDisconnection = [this]() {
      if (!m_universeClient->isConnected()) {
        m_cinematicOverlay->stop();
        String errMessage;
        if (auto disconnectReason = m_universeClient->disconnectReason())
          errMessage = strf("You were disconnected from the server for the following reason:\n{}", *disconnectReason);
        else
          errMessage = "Client-server connection no longer valid!";
        setError(errMessage);
        changeState(MainAppState::Title);
        return true;
      }

      return false;
    };

    if (checkDisconnection())
      return;

    m_mainInterface->preUpdate(dt);
    m_universeClient->update(dt);

    if (checkDisconnection())
      return;

    if (worldClient) {
      m_worldPainter->update(dt);
      auto& broadcastCallback = worldClient->broadcastCallback();
      if (!broadcastCallback) {
        broadcastCallback = [&](PlayerPtr player, StringView broadcast) -> bool {
          auto& view = broadcast.utf8();
          if (view.rfind(VoiceBroadcastPrefix.utf8(), 0) != NPos) {
            auto entityId = player->entityId();
            auto speaker = m_voice->speaker(connectionForEntity(entityId));
            speaker->entityId = entityId;
            speaker->name = player->name();
            speaker->position = player->mouthPosition();
            m_voice->receive(speaker, view.substr(VoiceBroadcastPrefix.utf8Size()));
          }
          return true;
        };
      }

      if (worldClient->inWorld()) {
        if (needstoSendVoice) {
          auto signature = Curve25519::sign(voiceData.ptr(), voiceData.size());
          std::string_view signatureView((char*)signature.data(), signature.size());
          std::string_view audioDataView(voiceData.ptr(), voiceData.size());
          auto broadcast = strf("data\0voice\0{}{}"s, signatureView, audioDataView);
          worldClient->sendSecretBroadcast(broadcast, true, false); // Already compressed by Opus.
        }
        if (auto mainPlayer = m_universeClient->mainPlayer()) {
          auto localSpeaker = m_voice->localSpeaker();
          localSpeaker->position = mainPlayer->position();
          localSpeaker->entityId = mainPlayer->entityId();
          localSpeaker->name = mainPlayer->name();
        }
        m_voice->setLocalSpeaker(worldClient->connection());
      }
      worldClient->setInteractiveHighlightMode(isActionTaken(InterfaceAction::ShowLabels));
    }
    if (m_benchmark.enabled)
      updateBenchmark(dt, worldClient);

    if (m_state != MainAppState::SinglePlayer && m_state != MainAppState::MultiPlayer)
      return;

    updateCamera(dt, worldClient);

    m_cinematicOverlay->update(dt);
    m_mainInterface->update(dt);
    m_mainMixer->update(dt, m_cinematicOverlay->muteSfx(), m_cinematicOverlay->muteMusic());
    m_mainMixer->setSpeed(GlobalTimescale);

    bool inputActive = m_mainInterface->textInputActive();
    m_input->setTextInputActive(inputActive);
    if (inputActive)
      app->setTextArea(m_mainInterface->paneManager()->keyboardCapturedWidget()->keyboardCaptureArea());
    else
      app->setTextArea();
    app->setAcceptingTextInput(inputActive);

    for (auto const& interactAction : m_player->pullInteractActions())
      m_mainInterface->handleInteractAction(interactAction);

    if (m_universeServer) {
      if (auto p2pNetworkingService = app->p2pNetworkingService()) {
        for (auto& p2pClient : p2pNetworkingService->acceptP2PConnections())
          m_universeServer->addClient(UniverseConnection(P2PPacketSocket::open(std::move(p2pClient))));
      }

      m_universeServer->setPause(m_mainInterface->escapeDialogOpen());
    }

    Vec2F aimPosition = m_player->aimPosition();
    float fps = app->renderFps();
    LogMap::set("client_render_rate", strf("{:4.2f} FPS ({:4.2f}ms)", fps, (1.0f / app->renderFps()) * 1000.0f));
    LogMap::set("client_update_rate", strf("{:4.2f}Hz", app->updateRate()));
    LogMap::set("player_pos", strf("[ ^#f45;{:4.2f}^reset;, ^#49f;{:4.2f}^reset; ]", m_player->position()[0], m_player->position()[1]));
    LogMap::set("player_vel", strf("[ ^#f45;{:4.2f}^reset;, ^#49f;{:4.2f}^reset; ]", m_player->velocity()[0], m_player->velocity()[1]));
    LogMap::set("player_aim", strf("[ ^#f45;{:4.2f}^reset;, ^#49f;{:4.2f}^reset; ]", aimPosition[0], aimPosition[1]));
    if (auto world = m_universeClient->worldClient()) {
      auto aim = Vec2I::floor(aimPosition);
      LogMap::set("tile_liquid_level", toString(world->liquidLevel(aim).level));
      LogMap::set("tile_dungeon_id", world->isTileProtected(aim) ? strf("^red;{}", world->dungeonId(aim)) : toString(world->dungeonId(aim)));
    }

    if (m_mainInterface->currentState() == MainInterface::ReturnToTitle)
      changeState(MainAppState::Title);

  } catch (std::exception& e) {
    setError("Exception caught in client main-loop", e);
  }
}

void ClientApplication::benchmarkRequestWarp(String label, WarpAction const& action, String actionName) {
  if (!m_universeClient || !m_universeClient->isConnected()) {
    m_benchmark.warnings.append(strf("Unable to request warp '{}' because the universe client is not connected", label));
    return;
  }

  BenchmarkWarpEvent event;
  event.label = std::move(label);
  event.action = std::move(actionName);
  event.fromWorldId = printWorldId(m_universeClient->playerWorld());
  event.requestedAtSeconds = monotonicSeconds() - m_benchmark.startedAtSeconds;
  m_benchmark.pendingWarpEvent = event;

  Logger::info("Benchmark warp request '{}' -> {}", event.label, event.action);
  m_universeClient->warpPlayer(action);
}

void ClientApplication::benchmarkRunAssetLoadSweep() {
  if (!m_root)
    return;

  auto assets = m_root->assets();
  if (!assets) {
    m_benchmark.warnings.append("Asset sweep skipped: no assets interface available");
    return;
  }

  auto sweepStartUs = Time::monotonicMicroseconds();
  m_benchmark.assetTypeStats.clear();
  m_benchmark.slowAssetSamples.clear();
  m_benchmark.assetSweepAttempted = 0;
  m_benchmark.assetSweepSucceeded = 0;
  m_benchmark.assetSweepFailed = 0;

  struct SweepExtension {
    char const* extension;
    char const* loadType;
  };

  static List<SweepExtension> const SweepExtensions = {
    {"png", "image"},
    {"json", "json"},
    {"config", "json"},
    {"frames", "json"},
    {"material", "json"},
    {"matmod", "json"},
    {"object", "json"},
    {"animation", "json"},
    {"particle", "json"},
    {"biome", "json"},
    {"dungeon", "json"},
    {"tech", "json"},
    {"questtemplate", "json"},
    {"recipe", "json"},
    {"activeitem", "json"},
    {"consumable", "json"},
    {"species", "json"},
    {"shipworld", "bytes"},
    {"lua", "bytes"},
    {"ogg", "audio"},
    {"wav", "audio"},
    {"ttf", "font"},
    {"otf", "font"}
  };

  StringSet seenPaths;
  List<pair<String, String>> sampledAssets;
  sampledAssets.reserve(m_benchmark.assetSampleCount);

  for (auto const& extension : SweepExtensions) {
    auto const& paths = assets->scanExtension(extension.extension);
    m_benchmark.assetCatalogCounts[extension.extension] = (uint64_t)paths.size();

    for (auto const& path : paths) {
      if (sampledAssets.size() >= m_benchmark.assetSampleCount)
        break;
      if (seenPaths.insert(path).second)
        sampledAssets.append({path, extension.loadType});
    }

    if (sampledAssets.size() >= m_benchmark.assetSampleCount)
      break;
  }

  m_benchmark.assetCatalogCounts["sampledAssets"] = (uint64_t)sampledAssets.size();
  m_benchmark.assetCatalogCounts["sampleLimit"] = m_benchmark.assetSampleCount;

  if (sampledAssets.empty()) {
    m_benchmark.warnings.append("Asset sweep found zero assets for configured extension set");
    return;
  }

  for (auto const& item : sampledAssets) {
    auto const& path = item.first;
    auto const& loadType = item.second;
    auto loadStartUs = Time::monotonicMicroseconds();

    bool success = true;
    String error;
    try {
      if (loadType == "image")
        assets->image(path);
      else if (loadType == "audio")
        assets->audio(path);
      else if (loadType == "font")
        assets->font(path);
      else if (loadType == "json")
        assets->json(path);
      else
        assets->bytes(path);
    } catch (std::exception const& e) {
      success = false;
      error = strf("{}", outputException(e, false));
    }

    double loadMs = (double)(Time::monotonicMicroseconds() - loadStartUs) / 1000.0;
    auto& stats = m_benchmark.assetTypeStats[loadType];
    ++stats.attempted;
    stats.totalMs += loadMs;
    stats.maxMs = std::max(stats.maxMs, loadMs);
    if (success)
      ++stats.succeeded;
    else
      ++stats.failed;

    BenchmarkSlowAssetSample sample;
    sample.path = path;
    sample.loadType = loadType;
    sample.loadMs = loadMs;
    sample.success = success;
    sample.error = std::move(error);
    m_benchmark.slowAssetSamples.append(std::move(sample));

    ++m_benchmark.assetSweepAttempted;
    if (success)
      ++m_benchmark.assetSweepSucceeded;
    else
      ++m_benchmark.assetSweepFailed;
  }

  m_benchmark.slowAssetSamples.sort([](BenchmarkSlowAssetSample const& a, BenchmarkSlowAssetSample const& b) {
    return a.loadMs > b.loadMs;
  });
  m_benchmark.slowAssetSamples.limitSizeBack((size_t)m_benchmark.maxSlowAssets);

  m_benchmark.assetSweepTotalMs = (double)(Time::monotonicMicroseconds() - sweepStartUs) / 1000.0;
  Logger::info(
      "Benchmark asset sweep complete: attempted={}, success={}, failed={}, total={}ms",
      m_benchmark.assetSweepAttempted,
      m_benchmark.assetSweepSucceeded,
      m_benchmark.assetSweepFailed,
      m_benchmark.assetSweepTotalMs);
}

bool ClientApplication::benchmarkIssueCommand(String const& command) {
  if (m_player && !m_player->inWorld())
    return false;

  if (m_player && m_player->isTeleporting())
    return false;

  if (m_universeClient && !m_universeClient->isConnected())
    return false;

  if (m_universeClient) {
    auto worldClient = m_universeClient->worldClient();
    if (!worldClient || !worldClient->inWorld())
      return false;
  }

  if (!m_mainInterface) {
    m_benchmark.warnings.append(strf("Stress command skipped (no main interface): {}", command));
    return false;
  }

  auto commandProcessor = m_mainInterface->commandProcessor();
  if (!commandProcessor) {
    m_benchmark.warnings.append(strf("Stress command skipped (no command processor): {}", command));
    return false;
  }

  if (m_benchmark.stressMode)
    ++m_benchmark.stressCommandsIssued;

  bool sawInvalidClientState = false;
  auto results = commandProcessor->handleCommand(command, false);
  for (auto const& result : results) {
    if (result.equals("Invalid client state", String::CaseInsensitive)) {
      sawInvalidClientState = true;
      continue;
    }

    if (!result.empty() && !result.equals(" "))
      Logger::info("Benchmark stress command '{}': {}", command, result);
  }

  return !sawInvalidClientState;
}

void ClientApplication::benchmarkResetStressActions() {
  double now = monotonicSeconds();
  m_benchmark.stressNextAiWaveAtSeconds = now + 0.5;
  m_benchmark.stressNextItemWaveAtSeconds = now + 0.35;
  m_benchmark.stressNextTerrainPulseAtSeconds = now + 0.9;
  m_benchmark.stressNextLiquidPulseAtSeconds = now + 0.6;
  m_benchmark.stressNextExplosionPulseAtSeconds = now + 1.6;
  m_benchmark.stressNextJumpAtSeconds = now + 0.75;
  m_benchmark.stressNextWeatherPulseAtSeconds = now + 3.0;
  m_benchmark.stressNextEntityTrimAtSeconds = now + 9.0;
  m_benchmark.stressTerrainRebuildPass = false;
  m_benchmark.stressWeatherForceEnabled = true;
  m_benchmark.stressWeatherCycleIndex = 0;
  m_benchmark.stressAnchorTile = {};
  m_benchmark.stressCommandAimPosition = {};
}

Vec2I ClientApplication::benchmarkStressAnchorTile(WorldClientPtr const& worldClient) {
  if (m_benchmark.stressAnchorTile)
    return *m_benchmark.stressAnchorTile;

  if (!worldClient || !m_player)
    return {};

  auto const& geometry = worldClient->geometry();
  int maxY = std::max<int>(2, (int)geometry.height() - 3);

  Vec2I anchor = Vec2I::floor(m_player->position());
  anchor[0] = geometry.xwrap(anchor[0]);
  anchor[1] = std::clamp(anchor[1], 2, maxY);

  for (int i = 0; i < 48; ++i) {
    int y = anchor[1] - i;
    if (y <= 2)
      break;

    if (worldClient->tileIsOccupied({anchor[0], y}, TileLayer::Foreground, true)) {
      anchor[1] = std::clamp(y + 2, 2, maxY);
      break;
    }
  }

  m_benchmark.stressAnchorTile = anchor;
  return anchor;
}

Vec2F ClientApplication::benchmarkStressCommandAim(WorldClientPtr const& worldClient) {
  if (worldClient && m_player) {
    auto const& geometry = worldClient->geometry();
    int maxY = std::max<int>(2, (int)geometry.height() - 3);
    Vec2I anchor = benchmarkStressAnchorTile(worldClient);
    return Vec2F((float)geometry.xwrap(anchor[0]) + 0.5f, (float)std::clamp(anchor[1], 2, maxY) + 0.5f);
  }

  if (m_benchmark.stressCommandAimPosition)
    return *m_benchmark.stressCommandAimPosition;

  if (m_player)
    return m_player->position() + m_player->feetOffset();

  return {};
}

Vec2F ClientApplication::benchmarkStressSpawnPosition(WorldClientPtr const& worldClient) {
  Vec2F spawnPos = benchmarkStressCommandAim(worldClient);

  if (worldClient) {
    auto const& geometry = worldClient->geometry();
    int maxY = std::max<int>(2, (int)geometry.height() - 3);
    spawnPos[0] = (float)geometry.xwrap((int)std::floor(spawnPos[0])) + 0.5f;
    spawnPos[1] = (float)std::clamp((int)std::floor(spawnPos[1]), 2, maxY) + 0.5f;
  }

  return spawnPos;
}

bool ClientApplication::benchmarkExecuteAtStressAnchor(function<void(WorldServer*, PlayerPtr const&, Vec2F const&)> const& action) {
  if (!m_benchmark.stressMode || !m_universeServer || !m_universeClient || !m_universeClient->clientContext())
    return false;

  auto worldClient = m_universeClient->worldClient();
  if (!worldClient || !worldClient->inWorld())
    return false;

  ConnectionId connectionId = m_universeClient->clientContext()->connectionId();
  if (connectionId == 0)
    return false;

  Vec2F spawnPos = benchmarkStressSpawnPosition(worldClient);
  bool executed = false;

  bool done = m_universeServer->executeForClient(connectionId, [&](WorldServer* world, PlayerPtr const& serverPlayer) {
      if (!world || !serverPlayer)
        return;

      action(world, serverPlayer, spawnPos);
      executed = true;
    });

  if (m_benchmark.stressMode)
    ++m_benchmark.stressCommandsIssued;

  return done && executed;
}

bool ClientApplication::benchmarkSpawnItemDirect(String const& itemName, uint64_t amount) {
  try {
    return benchmarkExecuteAtStressAnchor([&](WorldServer* world, PlayerPtr const&, Vec2F const& spawnPos) {
      auto itemDatabase = Root::singleton().itemDatabase();
      ItemDescriptor descriptor(itemName, amount, JsonObject());
      auto item = itemDatabase->item(descriptor, {}, {}, true);
      world->addEntity(ItemDrop::createRandomizedDrop(item, spawnPos));
    });
  } catch (std::exception const& e) {
    m_benchmark.warnings.append(strf(
        "Benchmark direct spawn item failed ('{}' x{}): {}",
        itemName,
        amount,
        outputException(e, false)));
    return false;
  }
}

bool ClientApplication::benchmarkSpawnNpcDirect(String const& species, String const& npcType, float level) {
  try {
    return benchmarkExecuteAtStressAnchor([&](WorldServer* world, PlayerPtr const&, Vec2F const& spawnPos) {
      auto npcDatabase = Root::singleton().npcDatabase();
      auto variant = npcDatabase->generateNpcVariant(species, npcType, level, Random::randu64(), JsonObject());
      auto npc = npcDatabase->createNpc(std::move(variant));
      npc->setPosition(spawnPos);
      world->addEntity(npc);
    });
  } catch (std::exception const& e) {
    m_benchmark.warnings.append(strf(
        "Benchmark direct spawn NPC failed ('{} {}' level={}): {}",
        species,
        npcType,
        level,
        outputException(e, false)));
    return false;
  }
}

bool ClientApplication::benchmarkSpawnMonsterDirect(String const& monsterType, float level) {
  try {
    return benchmarkExecuteAtStressAnchor([&](WorldServer* world, PlayerPtr const&, Vec2F const& spawnPos) {
      auto monsterDatabase = Root::singleton().monsterDatabase();
      auto monster = monsterDatabase->createMonster(monsterDatabase->randomMonster(monsterType, JsonObject()), level);
      monster->setPosition(spawnPos);
      world->addEntity(monster);
    });
  } catch (std::exception const& e) {
    m_benchmark.warnings.append(strf(
        "Benchmark direct spawn monster failed ('{}' level={}): {}",
        monsterType,
        level,
        outputException(e, false)));
    return false;
  }
}

bool ClientApplication::benchmarkSpawnLiquidDirect(String const& liquidName, float quantity) {
  try {
    auto liquidsDatabase = Root::singleton().liquidsDatabase();
    if (!liquidsDatabase->isLiquidName(liquidName))
      return false;

    LiquidId liquidId = liquidsDatabase->liquidId(liquidName);
    return benchmarkExecuteAtStressAnchor([&](WorldServer* world, PlayerPtr const&, Vec2F const& spawnPos) {
      world->modifyTile(Vec2I(spawnPos.floor()), PlaceLiquid{liquidId, quantity}, true);
    });
  } catch (std::exception const& e) {
    m_benchmark.warnings.append(strf(
        "Benchmark direct spawn liquid failed ('{}' quantity={}): {}",
        liquidName,
        quantity,
        outputException(e, false)));
    return false;
  }
}

bool ClientApplication::benchmarkSetWeatherDirect(String const& weatherName, bool force) {
  try {
    return benchmarkExecuteAtStressAnchor([&](WorldServer* world, PlayerPtr const&, Vec2F const&) {
      world->setWeather(weatherName, force);
    });
  } catch (std::exception const& e) {
    m_benchmark.warnings.append(strf(
        "Benchmark direct weather set failed ('{}' force={}): {}",
        weatherName,
        force,
        outputException(e, false)));
    return false;
  }
}

void ClientApplication::benchmarkSyncStressCommandAim(WorldClientPtr const& worldClient) {
  if (!m_benchmark.stressMode || !m_player)
    return;

  Vec2F aim = benchmarkStressCommandAim(worldClient);
  m_benchmark.stressCommandAimPosition = aim;
  m_player->aim(aim);

  if (m_universeServer && m_universeClient && m_universeClient->clientContext()) {
    ConnectionId connectionId = m_universeClient->clientContext()->connectionId();
    if (connectionId != 0) {
      m_universeServer->executeForClient(connectionId, [aim](WorldServer*, PlayerPtr serverPlayer) {
          if (serverPlayer)
            serverPlayer->aim(aim);
        });
    }
  }
}

void ClientApplication::benchmarkStressPlayerMovement(WorldClientPtr const&, double nowSeconds) {
  if (!m_player)
    return;

  double phaseSeconds = std::max(0.0, nowSeconds - m_benchmark.phaseStartedAtSeconds);
  float moveDirection = (float)std::sin(phaseSeconds * 1.35);
  float pace = (float)(0.65 + 0.35 * std::sin(phaseSeconds * 0.27 + 0.9));
  m_player->setMoveVector({moveDirection * pace, 0.0f});
  m_player->setShifting(std::fmod(phaseSeconds, 7.5) < 2.25);

  if (nowSeconds >= m_benchmark.stressNextJumpAtSeconds) {
    m_player->jump();
    ++m_benchmark.stressJumpPulses;
    double jitter = 0.75 + 0.35 * std::abs(std::sin((double)m_benchmark.stressJumpPulses * 0.73));
    m_benchmark.stressNextJumpAtSeconds = nowSeconds + m_benchmark.stressJumpIntervalSeconds * jitter;
  }
}

void ClientApplication::benchmarkStressSpawnAiWave() {
  static const char* Species[] = {"human", "avian", "apex", "hylotl", "floran", "glitch", "novakid"};
  uint64_t wave = m_benchmark.stressAiWaves++;

  uint64_t npcCount = std::max<uint64_t>(2, std::min<uint64_t>(8, m_benchmark.stressNpcCount / 6));
  uint64_t monsterCount = std::max<uint64_t>(3, std::min<uint64_t>(12, m_benchmark.stressMonsterCount / 6));
  npcCount += wave % 3;
  monsterCount += wave % 5;

  for (uint64_t i = 0; i < npcCount; ++i) {
    String species = Species[(size_t)((wave + i) % (sizeof(Species) / sizeof(Species[0])))];
    benchmarkSpawnNpcDirect(species, "villager", (float)(8 + (int)(wave % 5)));
  }

  for (uint64_t i = 0; i < monsterCount; ++i)
    benchmarkSpawnMonsterDirect("poptop", (float)(8 + (int)((wave + i) % 7)));
}

void ClientApplication::benchmarkStressSpawnItemWave() {
  uint64_t wave = m_benchmark.stressItemWaves++;
  uint64_t dropCount = std::max<uint64_t>(8, std::min<uint64_t>(24, m_benchmark.stressItemDrops / 8));
  dropCount += wave % 4;

  for (uint64_t i = 0; i < dropCount; ++i) {
    if (((i + wave) % 2) == 0)
      benchmarkSpawnItemDirect("torch", 1);
    else
      benchmarkSpawnItemDirect("copperore", 1);
  }
}

void ClientApplication::benchmarkStressTerrainPulse(WorldClientPtr const& worldClient) {
  if (!worldClient || !m_player)
    return;

  auto const& geometry = worldClient->geometry();
  int maxY = std::max<int>(2, (int)geometry.height() - 3);
  Vec2I anchor = benchmarkStressAnchorTile(worldClient);

  if (m_benchmark.stressTerrainPulses == 0) {
    auto fg = worldClient->material(anchor, TileLayer::Foreground);
    auto bg = worldClient->material(anchor, TileLayer::Background);
    if (fg != EmptyMaterialId && fg != NullMaterialId)
      m_benchmark.stressRebuildForeground = fg;
    if (bg != EmptyMaterialId && bg != NullMaterialId)
      m_benchmark.stressRebuildBackground = bg;
  }

  double phaseSeconds = std::max(0.0, monotonicSeconds() - m_benchmark.phaseStartedAtSeconds);
  int xOffset = (int)std::round(std::sin(phaseSeconds * 0.45) * 18.0);
  int yOffset = (int)std::round(std::cos(phaseSeconds * 0.63) * 4.0);

  Vec2I center(geometry.xwrap(anchor[0] + xOffset), std::clamp(anchor[1] - 6 + yOffset, 2, maxY));
  int halfWidth = 8;
  int halfHeight = 4;
  int pulseSeed = (int)m_benchmark.stressTerrainPulses;

  if (m_benchmark.stressTerrainRebuildPass) {
    TileModificationList replacements;
    for (int y = -halfHeight; y <= halfHeight; ++y) {
      for (int x = -halfWidth; x <= halfWidth; ++x) {
        if (((std::abs(x) + std::abs(y) + pulseSeed) % 3) == 0)
          continue;

        Vec2I pos(geometry.xwrap(center[0] + x), std::clamp(center[1] + y, 2, maxY));
        replacements.emplaceAppend(pos, PlaceMaterial{TileLayer::Foreground, m_benchmark.stressRebuildForeground, {}, TileCollisionOverride::None});
        if (((x - y + pulseSeed) % 3) == 0)
          replacements.emplaceAppend(pos, PlaceMaterial{TileLayer::Background, m_benchmark.stressRebuildBackground, {}, TileCollisionOverride::None});
      }
    }

    if (!replacements.empty()) {
      TileDamage replaceDamage(TileDamageType::Beamish, 3.0f, 4);
      auto failures = worldClient->replaceTiles(replacements, replaceDamage, false);
      if (replacements.size() >= failures.size())
        m_benchmark.stressTerrainTilesRebuilt += (uint64_t)(replacements.size() - failures.size());
    }
  } else {
    List<Vec2I> foregroundTiles;
    List<Vec2I> backgroundTiles;
    for (int y = -halfHeight; y <= halfHeight; ++y) {
      for (int x = -halfWidth; x <= halfWidth; ++x) {
        if (((std::abs(x) + std::abs(y) + pulseSeed) % 3) == 0)
          continue;

        Vec2I pos(geometry.xwrap(center[0] + x), std::clamp(center[1] + y, 2, maxY));
        if (worldClient->material(pos, TileLayer::Foreground) != EmptyMaterialId)
          foregroundTiles.append(pos);
        if ((((x + y + pulseSeed) % 2) == 0) && worldClient->material(pos, TileLayer::Background) != EmptyMaterialId)
          backgroundTiles.append(pos);
      }
    }

    Maybe<EntityId> sourceEntity;
    if (m_player)
      sourceEntity = m_player->entityId();

    if (!foregroundTiles.empty()) {
      TileDamage damage(TileDamageType::Explosive, 55.0f, 5);
      worldClient->damageTiles(foregroundTiles, TileLayer::Foreground, m_player->position(), damage, sourceEntity);
      m_benchmark.stressTerrainTilesDamaged += (uint64_t)foregroundTiles.size();
    }
    if (!backgroundTiles.empty()) {
      TileDamage damage(TileDamageType::Fire, 36.0f, 4);
      worldClient->damageTiles(backgroundTiles, TileLayer::Background, m_player->position(), damage, sourceEntity);
      m_benchmark.stressTerrainTilesDamaged += (uint64_t)backgroundTiles.size();
    }
  }

  m_benchmark.stressTerrainRebuildPass = !m_benchmark.stressTerrainRebuildPass;
  ++m_benchmark.stressTerrainPulses;
}

void ClientApplication::benchmarkStressLiquidPulse(WorldClientPtr const& worldClient) {
  if (!worldClient)
    return;

  List<LiquidId> availableLiquids;
  for (LiquidId liquidId : {m_benchmark.stressLiquidPrimary, m_benchmark.stressLiquidSecondary, m_benchmark.stressLiquidTertiary}) {
    if (liquidId != EmptyLiquidId && !availableLiquids.contains(liquidId))
      availableLiquids.append(liquidId);
  }

  if (availableLiquids.empty())
    return;

  auto const& geometry = worldClient->geometry();
  int maxY = std::max<int>(2, (int)geometry.height() - 3);
  Vec2I anchor = benchmarkStressAnchorTile(worldClient);

  int pulseSeed = (int)m_benchmark.stressLiquidPulses;
  int xOffset = (int)std::round(std::sin((double)pulseSeed * 0.45) * 12.0);
  int yOffset = (int)std::round(std::cos((double)pulseSeed * 0.37) * 3.0);
  Vec2I center(geometry.xwrap(anchor[0] + xOffset), std::clamp(anchor[1] - 2 + yOffset, 2, maxY));

  LiquidId primaryLiquid = availableLiquids[(size_t)(m_benchmark.stressLiquidPulses % availableLiquids.size())];
  LiquidId secondaryLiquid = availableLiquids[(size_t)((m_benchmark.stressLiquidPulses + 1) % availableLiquids.size())];

  TileModificationList liquidPlacements;
  for (int y = -3; y <= 3; ++y) {
    for (int x = -7; x <= 7; ++x) {
      if ((std::abs(x) + std::abs(y)) > 8 || ((x + y + pulseSeed) % 3) != 0)
        continue;

      Vec2I pos(geometry.xwrap(center[0] + x), std::clamp(center[1] + y, 2, maxY));
      liquidPlacements.emplaceAppend(pos, PlaceLiquid{primaryLiquid, 0.75f});
      if (((x * y + pulseSeed) % 11) == 0 && secondaryLiquid != primaryLiquid)
        liquidPlacements.emplaceAppend(pos, PlaceLiquid{secondaryLiquid, 0.45f});
    }
  }

  if (!liquidPlacements.empty()) {
    auto failures = worldClient->applyTileModifications(liquidPlacements, true);
    if (liquidPlacements.size() >= failures.size())
      m_benchmark.stressLiquidTileWrites += (uint64_t)(liquidPlacements.size() - failures.size());
  }

  ++m_benchmark.stressLiquidPulses;
}

void ClientApplication::benchmarkStressExplosionPulse(WorldClientPtr const& worldClient) {
  if (!worldClient || !m_player)
    return;

  auto const& geometry = worldClient->geometry();
  int maxY = std::max<int>(2, (int)geometry.height() - 3);
  Vec2I anchor = benchmarkStressAnchorTile(worldClient);

  int pulseSeed = (int)m_benchmark.stressExplosionPulses;
  int xOffset = (int)std::round(std::sin((double)pulseSeed * 0.65) * 20.0);
  Vec2I center(geometry.xwrap(anchor[0] + xOffset), std::clamp(anchor[1] + 2, 2, maxY));

  int radiusX = 9;
  int radiusY = 4;
  int rx2 = radiusX * radiusX;
  int ry2 = radiusY * radiusY;
  int rxy2 = rx2 * ry2;

  List<Vec2I> blastTiles;
  for (int y = -radiusY; y <= radiusY; ++y) {
    for (int x = -radiusX; x <= radiusX; ++x) {
      if ((x * x * ry2) + (y * y * rx2) > rxy2)
        continue;
      blastTiles.append({geometry.xwrap(center[0] + x), std::clamp(center[1] + y, 2, maxY)});
    }
  }

  if (!blastTiles.empty()) {
    Maybe<EntityId> sourceEntity;
    sourceEntity = m_player->entityId();

    TileDamage explosive(TileDamageType::Explosive, 140.0f, 6);
    worldClient->damageTiles(blastTiles, TileLayer::Foreground, m_player->position(), explosive, sourceEntity);

    TileDamage fire(TileDamageType::Fire, 52.0f, 5);
    worldClient->damageTiles(blastTiles, TileLayer::Background, m_player->position(), fire, sourceEntity);

    m_benchmark.stressTerrainTilesDamaged += (uint64_t)blastTiles.size() * 2;
  }

  benchmarkSpawnMonsterDirect("poptop", 15.0f);
  ++m_benchmark.stressExplosionPulses;
}

void ClientApplication::benchmarkTrimStressEntities(WorldClientPtr const& worldClient) {
  if (!worldClient || !m_benchmark.stressPrepared)
    return;

  benchmarkExecuteAtStressAnchor([&](WorldServer* world, PlayerPtr const&, Vec2F const& anchorPos) {
    RectF trimRegion = RectF::withCenter(anchorPos, Vec2F(320.0f, 220.0f));

    List<EntityId> itemDrops;
    List<EntityId> npcs;
    List<EntityId> monsters;

    world->forEachEntity(trimRegion, [&](EntityPtr const& entity) {
      if (!entity || !entity->inWorld())
        return;

      switch (entity->entityType()) {
        case EntityType::ItemDrop:
          itemDrops.append(entity->entityId());
          break;
        case EntityType::Npc:
          npcs.append(entity->entityId());
          break;
        case EntityType::Monster:
          monsters.append(entity->entityId());
          break;
        default:
          break;
      }
    });

    uint64_t keepItemDrops = std::max<uint64_t>(96, m_benchmark.stressItemDrops);
    uint64_t keepNpcs = std::max<uint64_t>(16, m_benchmark.stressNpcCount);
    uint64_t keepMonsters = std::max<uint64_t>(24, m_benchmark.stressMonsterCount);

    auto trimEntities = [&](List<EntityId>& ids, uint64_t keepCount, uint64_t& trimmedCounter) {
      if (ids.size() <= keepCount)
        return;

      size_t removeCount = ids.size() - keepCount;
      for (size_t i = 0; i < removeCount; ++i) {
        world->removeEntity(ids[i], true);
        ++trimmedCounter;
      }
    };

    trimEntities(itemDrops, keepItemDrops, m_benchmark.stressTrimmedItemDrops);
    trimEntities(npcs, keepNpcs, m_benchmark.stressTrimmedNpcs);
    trimEntities(monsters, keepMonsters, m_benchmark.stressTrimmedMonsters);
  });
}

void ClientApplication::benchmarkEnsureStressWeatherCycle() {
  if (!m_benchmark.stressMode || !m_benchmark.stressPrepared || !m_benchmark.stressWeatherCycle.empty())
    return;

  StringList availableWeather;
  benchmarkExecuteAtStressAnchor([&](WorldServer* world, PlayerPtr const&, Vec2F const&) {
    availableWeather = world->weatherList();
  });

  if (availableWeather.empty())
    return;

  struct RankedWeather {
    int score;
    String name;
  };

  auto scoreWeather = [](String const& weatherName) {
    int score = 0;
    struct MatchWeight {
      char const* token;
      int weight;
    };
    static MatchWeight const Weights[] = {
        {"storm", 8}, {"thunder", 8}, {"blizzard", 7}, {"snow", 5}, {"sand", 6},
        {"ash", 6}, {"wind", 5}, {"gust", 5}, {"rain", 4}, {"hail", 6},
        {"acid", 7}, {"toxic", 7}, {"meteor", 8}, {"ember", 6}, {"fire", 6}
    };

    for (auto const& weight : Weights) {
      if (weatherName.contains(weight.token, String::CaseInsensitive))
        score += weight.weight;
    }

    return score;
  };

  List<RankedWeather> ranked;
  for (auto const& weatherName : availableWeather)
    ranked.append({scoreWeather(weatherName), weatherName});

  ranked.sort([](RankedWeather const& a, RankedWeather const& b) {
    if (a.score != b.score)
      return a.score > b.score;
    return a.name < b.name;
  });

  m_benchmark.stressWeatherCycle.clear();
  for (auto const& weather : ranked) {
    if (weather.score <= 0)
      continue;
    if (!m_benchmark.stressWeatherCycle.contains(weather.name))
      m_benchmark.stressWeatherCycle.append(weather.name);
  }

  if (m_benchmark.stressWeatherCycle.empty()) {
    for (auto const& weatherName : availableWeather) {
      if (!m_benchmark.stressWeatherCycle.contains(weatherName))
        m_benchmark.stressWeatherCycle.append(weatherName);
    }
  }

  m_benchmark.stressWeatherCycleIndex = 0;
  Logger::info("Benchmark stress weather cycle: {}", m_benchmark.stressWeatherCycle.join(", "));
}

String ClientApplication::benchmarkNextStressWeather() {
  benchmarkEnsureStressWeatherCycle();
  if (m_benchmark.stressWeatherCycle.empty())
    return "rain";

  size_t index = m_benchmark.stressWeatherCycleIndex % m_benchmark.stressWeatherCycle.size();
  String weather = m_benchmark.stressWeatherCycle.at(index);
  m_benchmark.stressWeatherCycleIndex = (index + 1) % m_benchmark.stressWeatherCycle.size();
  return weather;
}

void ClientApplication::benchmarkUpdateStressActions(WorldClientPtr const& worldClient, String const& worldId) {
  if (!m_benchmark.stressMode || !worldClient || !worldClient->inWorld() || !m_player)
    return;

  if (!m_benchmark.stressPrepared) {
    m_player->setMoveVector(Vec2F());
    m_player->setShifting(false);
    return;
  }

  bool inAreaSweep = m_benchmark.phase == BenchmarkPhase::AreaSweepPrimary
      || m_benchmark.phase == BenchmarkPhase::AreaSweepShip
      || m_benchmark.phase == BenchmarkPhase::AreaSweepOrbitedWorld;
  if (!inAreaSweep) {
    m_player->setMoveVector(Vec2F());
    m_player->setShifting(false);
    return;
  }

  double now = monotonicSeconds();
  bool inShipWorld = benchmarkIsShipWorldId(worldId);
  bool inPlanetWorld = worldId.beginsWith("CelestialWorld:", String::CaseInsensitive);

  if (m_player->isDead())
    m_player->revive(m_player->position() + m_player->feetOffset());

  benchmarkStressPlayerMovement(worldClient, now);

  if (now >= m_benchmark.stressNextAiWaveAtSeconds) {
    benchmarkStressSpawnAiWave();
    double jitter = 0.82 + 0.36 * std::abs(std::sin((double)m_benchmark.stressAiWaves * 0.51));
    m_benchmark.stressNextAiWaveAtSeconds = now + m_benchmark.stressAiWaveIntervalSeconds * jitter;
  }

  if (now >= m_benchmark.stressNextItemWaveAtSeconds) {
    benchmarkStressSpawnItemWave();
    double jitter = 0.8 + 0.35 * std::abs(std::sin((double)m_benchmark.stressItemWaves * 0.63));
    m_benchmark.stressNextItemWaveAtSeconds = now + m_benchmark.stressItemWaveIntervalSeconds * jitter;
  }

  if (!inShipWorld && now >= m_benchmark.stressNextTerrainPulseAtSeconds) {
    benchmarkStressTerrainPulse(worldClient);
    double jitter = 0.8 + 0.4 * std::abs(std::sin((double)m_benchmark.stressTerrainPulses * 0.47));
    m_benchmark.stressNextTerrainPulseAtSeconds = now + m_benchmark.stressTerrainPulseIntervalSeconds * jitter;
  }

  if (!inShipWorld && now >= m_benchmark.stressNextLiquidPulseAtSeconds) {
    benchmarkStressLiquidPulse(worldClient);
    double jitter = 0.8 + 0.35 * std::abs(std::sin((double)m_benchmark.stressLiquidPulses * 0.31));
    m_benchmark.stressNextLiquidPulseAtSeconds = now + m_benchmark.stressLiquidPulseIntervalSeconds * jitter;
  }

  if (!inShipWorld && now >= m_benchmark.stressNextExplosionPulseAtSeconds) {
    benchmarkStressExplosionPulse(worldClient);
    double jitter = 0.82 + 0.35 * std::abs(std::sin((double)m_benchmark.stressExplosionPulses * 0.57));
    m_benchmark.stressNextExplosionPulseAtSeconds = now + m_benchmark.stressExplosionPulseIntervalSeconds * jitter;
  }

  if (now >= m_benchmark.stressNextEntityTrimAtSeconds) {
    benchmarkTrimStressEntities(worldClient);
    m_benchmark.stressNextEntityTrimAtSeconds = now + 9.0;
  }

  if (inPlanetWorld && now >= m_benchmark.stressNextWeatherPulseAtSeconds) {
    String nextWeather = benchmarkNextStressWeather();
    benchmarkSetWeatherDirect(nextWeather, m_benchmark.stressWeatherForceEnabled);
    m_benchmark.stressWeatherForceEnabled = !m_benchmark.stressWeatherForceEnabled;
    ++m_benchmark.stressWeatherPulses;
    m_benchmark.stressNextWeatherPulseAtSeconds = now + m_benchmark.stressWeatherPulseIntervalSeconds;
  }
}

void ClientApplication::benchmarkPrepareStressScene() {
  if (!m_benchmark.stressMode || m_benchmark.stressPrepared)
    return;

  if (monotonicSeconds() < m_benchmark.stressPrepareNotBeforeSeconds)
    return;

  if (!m_player || !m_player->inWorld() || m_player->isTeleporting())
    return;

  if (m_universeClient) {
    auto worldClient = m_universeClient->worldClient();
    if (!worldClient || !worldClient->inWorld())
      return;
  }

  if (m_player) {
    if (m_universeServer && m_universeClient && m_universeClient->clientContext()) {
      ConnectionId connectionId = m_universeClient->clientContext()->connectionId();
      if (connectionId != 0)
        m_universeServer->setAdmin(connectionId, true);
    }

    if (!m_player->isAdmin())
      benchmarkIssueCommand("/admin");

    // Apply immediately on the local player to avoid same-frame deaths while server state catches up.
    m_player->setAdmin(true);
    if (m_player->isDead())
      m_player->revive(m_player->position() + m_player->feetOffset());
    Logger::info("Benchmark stress: enabled admin mode to keep benchmark player alive");
  }

  if (!benchmarkIssueCommand("/respawnInWorld true"))
    return;

  String currentWorldId = m_universeClient ? printWorldId(m_universeClient->playerWorld()) : String();
  if (currentWorldId.beginsWith("CelestialWorld:", String::CaseInsensitive))
    if (!benchmarkSetWeatherDirect(benchmarkNextStressWeather(), true))
      return;

  if (m_benchmark.stressForceZoomOut && m_root && m_root->configuration()) {
    m_root->configuration()->set("zoomLevel", 1.0f);
    Logger::info("Benchmark stress: forced zoomLevel to 1.0x (default zoom-out)");
  }

  Logger::info(
      "Benchmark stress: spawning entities/items/liquids (items={}, npcs={}, monsters={}, liquids={})",
      m_benchmark.stressItemDrops,
      m_benchmark.stressNpcCount,
      m_benchmark.stressMonsterCount,
      m_benchmark.stressLiquidBursts);

  for (uint64_t i = 0; i < m_benchmark.stressItemDrops; ++i) {
    switch (i % 2) {
      case 0:
        if (!benchmarkSpawnItemDirect("torch", 1))
          return;
        break;
      default:
        if (!benchmarkSpawnItemDirect("copperore", 1))
          return;
        break;
    }
  }

  for (uint64_t i = 0; i < m_benchmark.stressNpcCount; ++i)
    if (!benchmarkSpawnNpcDirect("human", "villager", 8.0f))
      return;

  for (uint64_t i = 0; i < m_benchmark.stressMonsterCount; ++i)
    if (!benchmarkSpawnMonsterDirect("poptop", 8.0f))
      return;

  for (uint64_t i = 0; i < m_benchmark.stressLiquidBursts; ++i) {
    if (!benchmarkSpawnLiquidDirect("lava", 1.0f))
      return;
    if ((i % 2) == 0)
      if (!benchmarkSpawnLiquidDirect("poison", 1.0f))
        return;
  }

  if (m_root) {
    auto liquids = m_root->liquidsDatabase();
    if (liquids->isLiquidName("water"))
      m_benchmark.stressLiquidPrimary = liquids->liquidId("water");
    if (liquids->isLiquidName("lava"))
      m_benchmark.stressLiquidSecondary = liquids->liquidId("lava");
    if (liquids->isLiquidName("poison"))
      m_benchmark.stressLiquidTertiary = liquids->liquidId("poison");
  }

  benchmarkResetStressActions();
  m_benchmark.stressPrepared = true;
  m_benchmark.stressPrepareNotBeforeSeconds = 0.0;
}

void ClientApplication::updateBenchmark(float, WorldClientPtr const& worldClient) {
  if (!m_benchmark.enabled || m_benchmark.completed)
    return;

  if (!m_universeClient || !m_universeClient->isConnected()) {
    benchmarkFinalize("disconnected");
    benchmarkEnterPhase(BenchmarkPhase::Completed);
    return;
  }

  String worldId = printWorldId(m_universeClient->playerWorld());

  if (!m_benchmark.running) {
    if (!worldClient || !worldClient->inWorld()) {
      m_benchmark.phase = BenchmarkPhase::WaitingForWorld;
      return;
    }

    if (!benchmarkEnsureLateGameWorld(worldClient, worldId)) {
      m_benchmark.phase = BenchmarkPhase::WaitingForWorld;
      return;
    }

    bool inShipWorld = benchmarkIsShipWorldId(worldId);

    if (m_benchmark.stressMode && inShipWorld) {
      double now = monotonicSeconds();
      if (!m_benchmark.stressPreWarpRequested || now >= m_benchmark.stressPreWarpRetryAtSeconds) {
        m_benchmark.stressPreWarpRequested = true;
        m_benchmark.stressPreWarpRetryAtSeconds = now + 3.0;
        m_universeClient->warpPlayer(WarpAlias::OrbitedWorld, false);
        Logger::info("Benchmark stress: warping from ship to orbited world before stress scene setup");
      }
      m_benchmark.phase = BenchmarkPhase::WaitingForWorld;
      return;
    }

    m_benchmark.stressPreWarpRequested = false;
    m_benchmark.stressPreWarpRetryAtSeconds = 0.0;

    m_benchmark.running = true;
    m_benchmark.startedAtMonotonicUs = Time::monotonicMicroseconds();
    m_benchmark.startedAtEpochMs = (int64_t)Time::millisecondsSinceEpoch();
    m_benchmark.startedAtSeconds = monotonicSeconds();
    m_benchmark.phaseStartedAtSeconds = m_benchmark.startedAtSeconds;
    m_benchmark.stressPrepareNotBeforeSeconds = m_benchmark.startedAtSeconds + 2.0;
    m_benchmark.lastWorldId = worldId;
    m_benchmark.visitedWorldIds.append(worldId);
    benchmarkPrepareStressScene();
    benchmarkEnterPhase(BenchmarkPhase::Warmup);
    Logger::info("Benchmark started in world '{}'", worldId);
    return;
  }

  if (worldId != m_benchmark.lastWorldId) {
    m_benchmark.lastWorldId = worldId;
    m_benchmark.stressAnchorTile = {};
    m_benchmark.stressCommandAimPosition = {};
    m_benchmark.stressWeatherCycle.clear();
    m_benchmark.stressWeatherCycleIndex = 0;
    if (!m_benchmark.visitedWorldIds.contains(worldId))
      m_benchmark.visitedWorldIds.append(worldId);
  }

  if (m_benchmark.pendingWarpEvent) {
    bool worldChanged = worldId != m_benchmark.pendingWarpEvent->fromWorldId;
    if (worldChanged && worldClient && worldClient->inWorld()) {
      auto warpEvent = m_benchmark.pendingWarpEvent.take();
      warpEvent.toWorldId = worldId;
      warpEvent.completed = true;
      warpEvent.completedAtSeconds = monotonicSeconds() - m_benchmark.startedAtSeconds;
      warpEvent.durationMs = (warpEvent.completedAtSeconds - warpEvent.requestedAtSeconds) * 1000.0;
      m_benchmark.warpEvents.append(std::move(warpEvent));
    } else if (benchmarkPhaseElapsed(m_benchmark.warpTimeoutSeconds)) {
      auto warpEvent = m_benchmark.pendingWarpEvent.take();
      warpEvent.completed = false;
      warpEvent.completedAtSeconds = monotonicSeconds() - m_benchmark.startedAtSeconds;
      warpEvent.durationMs = (warpEvent.completedAtSeconds - warpEvent.requestedAtSeconds) * 1000.0;
      warpEvent.error = strf("Timed out after {}s while waiting for world transition", m_benchmark.warpTimeoutSeconds);
      m_benchmark.warnings.append(strf("Warp '{}' timed out after {}s", warpEvent.label, m_benchmark.warpTimeoutSeconds));
      m_benchmark.warpEvents.append(std::move(warpEvent));
    }
  }

  if (m_benchmark.stressMode && !m_benchmark.stressPrepared)
    benchmarkPrepareStressScene();

  benchmarkUpdateStressActions(worldClient, worldId);

  bool inAreaSweep = m_benchmark.phase == BenchmarkPhase::AreaSweepPrimary
      || m_benchmark.phase == BenchmarkPhase::AreaSweepShip
      || m_benchmark.phase == BenchmarkPhase::AreaSweepOrbitedWorld;

  if (inAreaSweep) {
    double now = monotonicSeconds();
    bool prewarmReady = now - m_benchmark.areaSweepEnteredAtSeconds >= m_benchmark.areaPrewarmSweepSeconds;
    uint64_t loadedSectors = worldClient ? (uint64_t)worldClient->loadedSectorCount() : 0;
    bool loadedReady = loadedSectors >= m_benchmark.minLoadedSectors;

    if (loadedReady) {
      if (m_benchmark.areaSweepReadySinceSeconds <= 0.0)
        m_benchmark.areaSweepReadySinceSeconds = now;
    } else {
      m_benchmark.areaSweepReadySinceSeconds = 0.0;
    }

    m_benchmark.areaSweepReadyForSampling = prewarmReady
        && loadedReady
        && (now - m_benchmark.areaSweepReadySinceSeconds >= m_benchmark.areaSettleSeconds);

    if (!m_benchmark.areaSweepReadyForSampling) {
      m_benchmark.phaseStartedAtSeconds = now;
      return;
    }
  } else {
    m_benchmark.areaSweepReadyForSampling = false;
    m_benchmark.areaSweepReadySinceSeconds = 0.0;
  }

  if (inAreaSweep && m_benchmark.forceCameraSweep) {
    float previousX = m_cameraXOffset;
    float previousY = m_cameraYOffset;

    double phaseSeconds = monotonicSeconds() - m_benchmark.phaseStartedAtSeconds;
    double angle = phaseSeconds * m_benchmark.cameraFrequencyHz * 6.2831853071795864769;
    m_cameraXOffset = (float)(std::sin(angle) * m_benchmark.cameraAmplitudeTiles);
    m_cameraYOffset = (float)(std::cos(angle * 0.73) * m_benchmark.cameraAmplitudeTiles * 0.45);
    m_cameraOffsetDownTime = 1.0f;
    m_snapBackCameraOffset = false;

    double dx = (double)m_cameraXOffset - previousX;
    double dy = (double)m_cameraYOffset - previousY;
    m_benchmark.areaDistanceSweptTiles += std::sqrt(dx * dx + dy * dy);
  }

  switch (m_benchmark.phase) {
    case BenchmarkPhase::Warmup:
      if (benchmarkPhaseElapsed(m_benchmark.warmupSeconds))
        benchmarkEnterPhase(BenchmarkPhase::AreaSweepPrimary);
      break;

    case BenchmarkPhase::AreaSweepPrimary:
      if (benchmarkPhaseElapsed(m_benchmark.areaPrimarySeconds)) {
        if (benchmarkIsShipWorldId(worldId)) {
          m_benchmark.warnings.append("Primary sweep completed while already in ship world; skipping warp to own ship");
          benchmarkEnterPhase(BenchmarkPhase::AreaSweepShip);
        } else {
          benchmarkRequestWarp("to_own_ship", WarpAlias::OwnShip, "OwnShip");
          benchmarkEnterPhase(BenchmarkPhase::WarpToShip);
        }
      }
      break;

    case BenchmarkPhase::WarpToShip:
      if (!m_benchmark.pendingWarpEvent)
        benchmarkEnterPhase(BenchmarkPhase::AreaSweepShip);
      break;

    case BenchmarkPhase::AreaSweepShip:
      if (benchmarkPhaseElapsed(m_benchmark.areaShipSeconds)) {
        benchmarkRequestWarp("to_orbited_world", WarpAlias::OrbitedWorld, "OrbitedWorld");
        benchmarkEnterPhase(BenchmarkPhase::WarpToOrbitedWorld);
      }
      break;

    case BenchmarkPhase::WarpToOrbitedWorld:
      if (!m_benchmark.pendingWarpEvent)
        benchmarkEnterPhase(BenchmarkPhase::AreaSweepOrbitedWorld);
      break;

    case BenchmarkPhase::AreaSweepOrbitedWorld:
      if (benchmarkPhaseElapsed(m_benchmark.areaOrbitedSeconds)) {
        if (m_benchmark.assetScanEnabled)
          benchmarkEnterPhase(BenchmarkPhase::AssetLoadSweep);
        else
          benchmarkEnterPhase(BenchmarkPhase::Finalize);
      }
      break;

    case BenchmarkPhase::AssetLoadSweep:
      benchmarkRunAssetLoadSweep();
      benchmarkEnterPhase(BenchmarkPhase::Finalize);
      break;

    case BenchmarkPhase::Finalize:
      benchmarkFinalize("completed");
      benchmarkEnterPhase(BenchmarkPhase::Completed);
      break;

    default:
      break;
  }
}

void ClientApplication::benchmarkRecordFrameSample(
    WorldClientPtr const& worldClient,
    uint64_t frameUs,
    uint64_t worldClientUs,
    uint64_t worldPainterUs,
    uint64_t worldTotalUs,
    uint64_t interfaceUs) {
  if (!m_benchmark.enabled || !m_benchmark.running || m_benchmark.completed)
    return;

  bool inAreaSweep = m_benchmark.phase == BenchmarkPhase::AreaSweepPrimary
      || m_benchmark.phase == BenchmarkPhase::AreaSweepShip
      || m_benchmark.phase == BenchmarkPhase::AreaSweepOrbitedWorld;
  if (m_benchmark.sampleOnlyReadySectors && inAreaSweep && !m_benchmark.areaSweepReadyForSampling)
    return;

  BenchmarkFrameSample sample;
  sample.timeSeconds = monotonicSeconds() - m_benchmark.startedAtSeconds;
  sample.frameMs = (double)frameUs / 1000.0;
  sample.worldClientMs = (double)worldClientUs / 1000.0;
  sample.worldPainterMs = (double)worldPainterUs / 1000.0;
  sample.worldTotalMs = (double)worldTotalUs / 1000.0;
  sample.interfaceMs = (double)interfaceUs / 1000.0;
  sample.renderFps = appController()->renderFps();
  sample.phase = benchmarkPhaseName(m_benchmark.phase);
  sample.worldId = m_universeClient && m_universeClient->isConnected() ? printWorldId(m_universeClient->playerWorld()) : "Disconnected";
  if (worldClient) {
    sample.loadedSectors = (uint64_t)worldClient->loadedSectorCount();
    sample.deferredSectorUnloads = (uint64_t)worldClient->deferredSectorUnloads();
  }

  m_benchmark.frameSamples.append(std::move(sample));
  ++m_benchmark.frameCount;

  double frameMs = (double)frameUs / 1000.0;
  if (frameMs > 16.6667)
    ++m_benchmark.framesOver16Ms;
  if (frameMs > 33.3333)
    ++m_benchmark.framesOver33Ms;
  if (frameMs > 50.0)
    ++m_benchmark.framesOver50Ms;
  if (frameMs > 100.0)
    ++m_benchmark.framesOver100Ms;
  if (worldTotalUs == 0)
    ++m_benchmark.framesWithoutWorldTiming;
}

void ClientApplication::benchmarkFinalize(String reason) {
  if (m_benchmark.completed)
    return;

  m_benchmark.completed = true;
  m_benchmark.completionReason = std::move(reason);

  if (m_player) {
    m_player->setMoveVector(Vec2F());
    m_player->setShifting(false);
  }

  if (m_benchmark.outputPath.empty()) {
    if (m_root)
      m_benchmark.outputPath = m_root->toStoragePath(strf("graphics-benchmark-{}.json", Time::millisecondsSinceEpoch()));
    else
      m_benchmark.outputPath = strf("graphics-benchmark-{}.json", Time::millisecondsSinceEpoch());
  }

  auto percentile = [](List<double> values, double percentileValue) {
    if (values.empty())
      return 0.0;

    values.sort();
    double index = std::clamp(percentileValue, 0.0, 1.0) * (double)(values.count() - 1);
    size_t low = (size_t)std::floor(index);
    size_t high = (size_t)std::ceil(index);
    if (low == high)
      return values[low];
    double blend = index - (double)low;
    return values[low] * (1.0 - blend) + values[high] * blend;
  };

  auto summarize = [&](List<double> const& values) -> JsonObject {
    JsonObject summary{
      {"count", (uint64_t)values.count()},
      {"minMs", 0.0},
      {"maxMs", 0.0},
      {"avgMs", 0.0},
      {"p50Ms", 0.0},
      {"p90Ms", 0.0},
      {"p95Ms", 0.0},
      {"p99Ms", 0.0}
    };

    if (values.empty())
      return summary;

    List<double> sorted = values.sorted();
    double total = 0.0;
    for (auto v : values)
      total += v;

    summary["minMs"] = sorted.first();
    summary["maxMs"] = sorted.last();
    summary["avgMs"] = total / (double)values.count();
    summary["p50Ms"] = percentile(values, 0.50);
    summary["p90Ms"] = percentile(values, 0.90);
    summary["p95Ms"] = percentile(values, 0.95);
    summary["p99Ms"] = percentile(values, 0.99);
    return summary;
  };

  auto summarizeFps = [&](List<double> const& values) -> JsonObject {
    JsonObject summary{
      {"count", (uint64_t)values.count()},
      {"minFps", 0.0},
      {"maxFps", 0.0},
      {"avgFps", 0.0},
      {"p10Fps", 0.0},
      {"p50Fps", 0.0},
      {"p90Fps", 0.0},
      {"p95Fps", 0.0}
    };

    if (values.empty())
      return summary;

    List<double> sorted = values.sorted();
    double total = 0.0;
    for (auto v : values)
      total += v;

    summary["minFps"] = sorted.first();
    summary["maxFps"] = sorted.last();
    summary["avgFps"] = total / (double)values.count();
    summary["p10Fps"] = percentile(values, 0.10);
    summary["p50Fps"] = percentile(values, 0.50);
    summary["p90Fps"] = percentile(values, 0.90);
    summary["p95Fps"] = percentile(values, 0.95);
    return summary;
  };

  JsonArray sampleTimeSeconds;
  JsonArray samplePhase;
  JsonArray sampleWorldId;
  JsonArray sampleFrameMs;
  JsonArray sampleWorldClientMs;
  JsonArray sampleWorldPainterMs;
  JsonArray sampleWorldTotalMs;
  JsonArray sampleInterfaceMs;
  JsonArray sampleRenderFps;
  JsonArray sampleLoadedSectors;
  JsonArray sampleDeferredUnloads;

  List<double> frameDurations;
  List<double> worldClientDurations;
  List<double> worldPainterDurations;
  List<double> worldTotalDurations;
  List<double> interfaceDurations;
  List<double> renderFpsValues;

  for (auto const& sample : m_benchmark.frameSamples) {
    sampleTimeSeconds.append(sample.timeSeconds);
    samplePhase.append(sample.phase);
    sampleWorldId.append(sample.worldId);
    sampleFrameMs.append(sample.frameMs);
    sampleWorldClientMs.append(sample.worldClientMs);
    sampleWorldPainterMs.append(sample.worldPainterMs);
    sampleWorldTotalMs.append(sample.worldTotalMs);
    sampleInterfaceMs.append(sample.interfaceMs);
    sampleRenderFps.append(sample.renderFps);
    sampleLoadedSectors.append(sample.loadedSectors);
    sampleDeferredUnloads.append(sample.deferredSectorUnloads);

    frameDurations.append(sample.frameMs);
    worldClientDurations.append(sample.worldClientMs);
    worldPainterDurations.append(sample.worldPainterMs);
    worldTotalDurations.append(sample.worldTotalMs);
    interfaceDurations.append(sample.interfaceMs);
    if (sample.renderFps > 0.0)
      renderFpsValues.append(sample.renderFps);
  }

  List<double> frameDeltaJitter;
  if (frameDurations.count() > 1) {
    frameDeltaJitter.reserve(frameDurations.count() - 1);
    for (size_t i = 1; i < frameDurations.count(); ++i)
      frameDeltaJitter.append(std::abs(frameDurations[i] - frameDurations[i - 1]));
  }

  JsonArray visitedWorlds;
  for (auto const& worldId : m_benchmark.visitedWorldIds)
    visitedWorlds.append(worldId);

  JsonArray warpEvents;
  for (auto const& warpEvent : m_benchmark.warpEvents) {
    warpEvents.append(JsonObject{
      {"label", warpEvent.label},
      {"action", warpEvent.action},
      {"fromWorldId", warpEvent.fromWorldId},
      {"toWorldId", warpEvent.toWorldId},
      {"requestedAtSeconds", warpEvent.requestedAtSeconds},
      {"completedAtSeconds", warpEvent.completedAtSeconds},
      {"durationMs", warpEvent.durationMs},
      {"completed", warpEvent.completed},
      {"error", warpEvent.error}
    });
  }

  JsonObject assetTypeStats;
  for (auto const& pair : m_benchmark.assetTypeStats) {
    auto const& stats = pair.second;
    double avgMs = stats.attempted ? stats.totalMs / (double)stats.attempted : 0.0;
    assetTypeStats[pair.first] = JsonObject{
      {"attempted", stats.attempted},
      {"succeeded", stats.succeeded},
      {"failed", stats.failed},
      {"totalMs", stats.totalMs},
      {"avgMs", avgMs},
      {"maxMs", stats.maxMs}
    };
  }

  JsonObject assetCatalogCounts;
  for (auto const& pair : m_benchmark.assetCatalogCounts)
    assetCatalogCounts[pair.first] = pair.second;

  JsonArray slowAssets;
  for (auto const& sample : m_benchmark.slowAssetSamples) {
    slowAssets.append(JsonObject{
      {"path", sample.path},
      {"loadType", sample.loadType},
      {"loadMs", sample.loadMs},
      {"success", sample.success},
      {"error", sample.error}
    });
  }

  JsonArray warnings;
  for (auto const& warning : m_benchmark.warnings)
    warnings.append(warning);

  auto frameCount = std::max<uint64_t>(1, m_benchmark.frameCount);
  auto percentage = [frameCount](uint64_t value) {
    return (double)value * 100.0 / (double)frameCount;
  };

  int64_t finishedAtEpochMs = (int64_t)Time::millisecondsSinceEpoch();
  double totalDurationSeconds = monotonicSeconds() - m_benchmark.startedAtSeconds;
  double targetFrameMs = 1000.0 / 60.0;
  double frameP95 = percentile(frameDurations, 0.95);
  double frameP99 = percentile(frameDurations, 0.99);
  double fpsP50 = percentile(renderFpsValues, 0.50);
  double fpsP10 = percentile(renderFpsValues, 0.10);
  double frameJitterP95 = percentile(frameDeltaJitter, 0.95);
  double frameJitterP99 = percentile(frameDeltaJitter, 0.99);

  bool fpsTargetMet = !renderFpsValues.empty() && fpsP50 >= 60.0 && fpsP10 >= 50.0;
  bool frameTargetMet = frameP95 <= (targetFrameMs * 1.10) && frameP99 <= 33.3333;
  bool steady60TargetMet = fpsTargetMet && frameTargetMet;

  double fpsScore = std::clamp(fpsP50 / 60.0, 0.0, 1.5);
  double lowFpsScore = std::clamp(fpsP10 / 60.0, 0.0, 1.0);
  double frameScore = std::clamp(targetFrameMs / std::max(targetFrameMs, frameP95), 0.0, 1.0);
  double stutterPenalty = 1.0 - std::clamp((double)m_benchmark.framesOver50Ms / (double)frameCount, 0.0, 1.0);
  double benchmarkScore = std::clamp(
      (fpsScore * 0.35) + (lowFpsScore * 0.20) + (frameScore * 0.30) + (stutterPenalty * 0.15),
      0.0,
      1.25);

  JsonObject report{
    {"benchmark", JsonObject{
      {"name", "graphics_vulkan_world_asset_benchmark"},
      {"version", 1},
      {"result", m_benchmark.completionReason},
      {"rendererId", renderer() ? renderer()->rendererId() : String("unknown")},
      {"outputPath", m_benchmark.outputPath},
      {"startedAtEpochMs", m_benchmark.startedAtEpochMs},
      {"finishedAtEpochMs", finishedAtEpochMs},
      {"durationSeconds", totalDurationSeconds},
      {"visitedWorldIds", visitedWorlds},
      {"warnings", warnings}
    }},
    {"settings", JsonObject{
      {"warmupSeconds", m_benchmark.warmupSeconds},
      {"areaPrimarySeconds", m_benchmark.areaPrimarySeconds},
      {"areaShipSeconds", m_benchmark.areaShipSeconds},
      {"areaOrbitedSeconds", m_benchmark.areaOrbitedSeconds},
      {"areaPrewarmSweepSeconds", m_benchmark.areaPrewarmSweepSeconds},
      {"warpTimeoutSeconds", m_benchmark.warpTimeoutSeconds},
      {"areaSettleSeconds", m_benchmark.areaSettleSeconds},
      {"minLoadedSectors", m_benchmark.minLoadedSectors},
      {"sampleOnlyReadySectors", m_benchmark.sampleOnlyReadySectors},
      {"cameraSweepEnabled", m_benchmark.forceCameraSweep},
      {"cameraAmplitudeTiles", m_benchmark.cameraAmplitudeTiles},
      {"cameraFrequencyHz", m_benchmark.cameraFrequencyHz},
      {"assetSweepEnabled", m_benchmark.assetScanEnabled},
      {"assetSampleCount", m_benchmark.assetSampleCount},
      {"maxSlowAssets", m_benchmark.maxSlowAssets},
      {"autoQuit", m_benchmark.autoQuit},
      {"isolateStorage", m_benchmark.isolateStorage},
      {"benchmarkStorageDirectory", m_benchmark.benchmarkStorageDirectory},
      {"benchmarkBootConfigPath", m_benchmark.benchmarkBootConfigPath},
      {"scenarioPlayerUuid", m_benchmark.scenarioPlayerUuid ? m_benchmark.scenarioPlayerUuid->hex() : String()},
      {"requireLateGameWorld", m_benchmark.requireLateGameWorld},
      {"preferTerrestrialLateGameWorld", m_benchmark.preferTerrestrialLateGameWorld},
      {"lateGameThreatLevel", m_benchmark.lateGameThreatLevel},
      {"lateGameTargetWorldId", m_benchmark.lateGameTargetWorldId},
      {"stressMode", m_benchmark.stressMode},
      {"stressUseExtendedDurations", m_benchmark.stressUseExtendedDurations},
      {"stressZoomOut", m_benchmark.stressForceZoomOut},
      {"stressItemDrops", m_benchmark.stressItemDrops},
      {"stressNpcCount", m_benchmark.stressNpcCount},
      {"stressMonsterCount", m_benchmark.stressMonsterCount},
      {"stressLiquidBursts", m_benchmark.stressLiquidBursts},
      {"stressAiWaveIntervalSeconds", m_benchmark.stressAiWaveIntervalSeconds},
      {"stressItemWaveIntervalSeconds", m_benchmark.stressItemWaveIntervalSeconds},
      {"stressTerrainPulseIntervalSeconds", m_benchmark.stressTerrainPulseIntervalSeconds},
      {"stressLiquidPulseIntervalSeconds", m_benchmark.stressLiquidPulseIntervalSeconds},
      {"stressExplosionPulseIntervalSeconds", m_benchmark.stressExplosionPulseIntervalSeconds},
      {"stressJumpIntervalSeconds", m_benchmark.stressJumpIntervalSeconds},
      {"stressWeatherPulseIntervalSeconds", m_benchmark.stressWeatherPulseIntervalSeconds}
    }},
    {"summary", JsonObject{
      {"frameCount", m_benchmark.frameCount},
      {"scenarioLateGameReady", m_benchmark.scenarioLateGameReady},
      {"areaDistanceSweptTiles", m_benchmark.areaDistanceSweptTiles},
      {"timings", JsonObject{
        {"frame", summarize(frameDurations)},
        {"worldClient", summarize(worldClientDurations)},
        {"worldPainter", summarize(worldPainterDurations)},
        {"worldTotal", summarize(worldTotalDurations)},
        {"interface", summarize(interfaceDurations)}
      }},
      {"stutter", JsonObject{
        {"framesOver16Ms", m_benchmark.framesOver16Ms},
        {"framesOver33Ms", m_benchmark.framesOver33Ms},
        {"framesOver50Ms", m_benchmark.framesOver50Ms},
        {"framesOver100Ms", m_benchmark.framesOver100Ms},
        {"framesWithoutWorldTiming", m_benchmark.framesWithoutWorldTiming},
        {"percentOver16Ms", percentage(m_benchmark.framesOver16Ms)},
        {"percentOver33Ms", percentage(m_benchmark.framesOver33Ms)},
        {"percentOver50Ms", percentage(m_benchmark.framesOver50Ms)},
        {"percentOver100Ms", percentage(m_benchmark.framesOver100Ms)}
      }},
      {"performance", JsonObject{
        {"benchmarkScore", benchmarkScore},
        {"steady60TargetMet", steady60TargetMet},
        {"targets", JsonObject{
          {"fpsTargetMet", fpsTargetMet},
          {"frameTimeTargetMet", frameTargetMet},
          {"targetFps", 60.0},
          {"targetFrameMs", targetFrameMs}
        }},
        {"fps", summarizeFps(renderFpsValues)},
        {"framePacing", JsonObject{
          {"p95DeltaMs", frameJitterP95},
          {"p99DeltaMs", frameJitterP99}
        }}
      }},
      {"warpEvents", warpEvents},
      {"stressLoad", JsonObject{
        {"commandsIssued", m_benchmark.stressCommandsIssued},
        {"aiWaves", m_benchmark.stressAiWaves},
        {"itemWaves", m_benchmark.stressItemWaves},
        {"terrainPulses", m_benchmark.stressTerrainPulses},
        {"liquidPulses", m_benchmark.stressLiquidPulses},
        {"explosionPulses", m_benchmark.stressExplosionPulses},
        {"jumpPulses", m_benchmark.stressJumpPulses},
        {"weatherPulses", m_benchmark.stressWeatherPulses},
        {"terrainTilesDamaged", m_benchmark.stressTerrainTilesDamaged},
        {"terrainTilesRebuilt", m_benchmark.stressTerrainTilesRebuilt},
        {"liquidTileWrites", m_benchmark.stressLiquidTileWrites},
        {"trimmedItemDrops", m_benchmark.stressTrimmedItemDrops},
        {"trimmedNpcs", m_benchmark.stressTrimmedNpcs},
        {"trimmedMonsters", m_benchmark.stressTrimmedMonsters}
      }},
      {"assetSweep", JsonObject{
        {"attempted", m_benchmark.assetSweepAttempted},
        {"succeeded", m_benchmark.assetSweepSucceeded},
        {"failed", m_benchmark.assetSweepFailed},
        {"totalMs", m_benchmark.assetSweepTotalMs},
        {"catalogCountsByExtension", assetCatalogCounts},
        {"typeStats", assetTypeStats},
        {"slowestAssets", slowAssets}
      }}
    }},
    {"rawSamples", JsonObject{
      {"timeSeconds", sampleTimeSeconds},
      {"phase", samplePhase},
      {"worldId", sampleWorldId},
      {"frameMs", sampleFrameMs},
      {"worldClientMs", sampleWorldClientMs},
      {"worldPainterMs", sampleWorldPainterMs},
      {"worldTotalMs", sampleWorldTotalMs},
      {"interfaceMs", sampleInterfaceMs},
      {"renderFps", sampleRenderFps},
      {"loadedSectors", sampleLoadedSectors},
      {"deferredSectorUnloads", sampleDeferredUnloads}
    }}
  };

  try {
    File::writeFile(Json(report).printJson(2, true), m_benchmark.outputPath);
    Logger::info("Benchmark report written to '{}'", m_benchmark.outputPath);
  } catch (std::exception const& e) {
    Logger::error("Failed to write benchmark report '{}': {}", m_benchmark.outputPath, outputException(e, false));
  }

  if (m_benchmark.autoQuit)
    changeState(MainAppState::Quit);
}

bool ClientApplication::isActionTaken(InterfaceAction action) const {
  for (auto keyEvent : m_heldKeyEvents) {
    if (m_guiContext->actions(keyEvent).contains(action))
      return true;
  }

  return false;
}

bool ClientApplication::isActionTakenEdge(InterfaceAction action) const {
  for (auto keyEvent : m_edgeKeyEvents) {
    if (m_guiContext->actions(keyEvent).contains(action))
      return true;
  }

  return false;
}

void ClientApplication::updateCamera(float dt, WorldClientPtr const& worldClient) {
  if (!worldClient)
    return;

  WorldCamera& camera = m_worldPainter->camera();
  camera.update(dt);

  if (m_mainInterface->fixedCamera())
    return;

  auto assets = m_root->assets();

  const float triggerRadius = 100.0f;
  const float deadzone = 0.1f;
  const float panFactor = 1.5f;
  float cameraSpeedFactor = 30.0f / m_root->configuration()->get("cameraSpeedFactor").toFloat();
  cameraSpeedFactor /= (dt * 60.f);

  auto playerCameraPosition = m_player->cameraPosition();

  if (isActionTaken(InterfaceAction::CameraShift)) {
    m_snapBackCameraOffset = false;
    m_cameraOffsetDownTime += dt;
    Vec2F aim = worldClient->geometry().diff(m_mainInterface->cursorWorldPosition(), playerCameraPosition);

    float magnitude = aim.magnitude() / (triggerRadius / camera.pixelRatio());
    if (magnitude > deadzone) {
      float cameraXOffset = aim.x() / magnitude;
      float cameraYOffset = aim.y() / magnitude;
      magnitude = (magnitude - deadzone) / (1.0 - deadzone);
      if (magnitude > 1)
        magnitude = 1;
      cameraXOffset *= magnitude * 0.5f * camera.pixelRatio() * panFactor;
      cameraYOffset *= magnitude * 0.5f * camera.pixelRatio() * panFactor;
      m_cameraXOffset = (m_cameraXOffset * (cameraSpeedFactor - 1.0) + cameraXOffset) / cameraSpeedFactor;
      m_cameraYOffset = (m_cameraYOffset * (cameraSpeedFactor - 1.0) + cameraYOffset) / cameraSpeedFactor;
    }
  } else {
    if (m_cameraOffsetDownTime > 0.0f && m_cameraOffsetDownTime < 0.333333f)
      m_snapBackCameraOffset = true;
    if (m_snapBackCameraOffset) {
      m_cameraXOffset = (m_cameraXOffset * (cameraSpeedFactor - 1.0)) / cameraSpeedFactor;
      m_cameraYOffset = (m_cameraYOffset * (cameraSpeedFactor - 1.0)) / cameraSpeedFactor;
    }
    m_cameraOffsetDownTime = 0.f;
  }
  Vec2F newCameraPosition;

  newCameraPosition.setX(playerCameraPosition.x());
  newCameraPosition.setY(playerCameraPosition.y());

  auto baseCamera = newCameraPosition;

  const float cameraSmoothRadius = assets->json("/interface.config:cameraSmoothRadius").toFloat();
  const float cameraSmoothFactor = assets->json("/interface.config:cameraSmoothFactor").toFloat();

  auto cameraSmoothDistance = worldClient->geometry().diff(m_cameraPositionSmoother, newCameraPosition).magnitude();
  if (cameraSmoothDistance > cameraSmoothRadius) {
    auto cameraDelta = worldClient->geometry().diff(m_cameraPositionSmoother, newCameraPosition);
    m_cameraPositionSmoother = newCameraPosition + cameraDelta.normalized() * cameraSmoothRadius;
    m_cameraSmoothDelta = {};
  }

  auto cameraDelta = worldClient->geometry().diff(m_cameraPositionSmoother, newCameraPosition);
  if (cameraDelta.magnitude() > assets->json("/interface.config:cameraSmoothDeadzone").toFloat())
    newCameraPosition = newCameraPosition + cameraDelta * (cameraSmoothFactor - 1.0) / cameraSmoothFactor;
  m_cameraPositionSmoother = newCameraPosition;

  newCameraPosition.setX(newCameraPosition.x() + m_cameraXOffset / camera.pixelRatio());
  newCameraPosition.setY(newCameraPosition.y() + m_cameraYOffset / camera.pixelRatio());

  auto smoothDelta = newCameraPosition - baseCamera;

  m_worldPainter->setCameraPosition(worldClient->geometry(), baseCamera + (smoothDelta + m_cameraSmoothDelta) * 0.5f);
  m_cameraSmoothDelta = smoothDelta;

  worldClient->setClientWindow(camera.worldTileRect());
}

}

STAR_MAIN_APPLICATION(Star::ClientApplication);
