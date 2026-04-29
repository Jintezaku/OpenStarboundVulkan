#pragma once

#include <cstdlib>

#include "StarFile.hpp"
#include "StarString.hpp"

namespace Star {

inline bool platformEnvEnabled(char const* name) {
  if (auto value = getenv(name)) {
    auto normalized = String(value).trim().toLower();
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
  }

  return false;
}

inline bool isSteamDeck() {
#ifdef STAR_SYSTEM_LINUX
  if (platformEnvEnabled("OPENSTARBOUND_FORCE_STEAM_DECK") || platformEnvEnabled("SteamDeck") || platformEnvEnabled("STEAM_DECK"))
    return true;

  auto readSystemFile = [](String const& path) {
    try {
      if (File::isFile(path))
        return File::readFileString(path).trim();
    } catch (...) {
    }

    return String{};
  };

  String vendor = readSystemFile("/sys/devices/virtual/dmi/id/sys_vendor");
  String product = readSystemFile("/sys/devices/virtual/dmi/id/product_name");
  String board = readSystemFile("/sys/devices/virtual/dmi/id/board_name");

  return vendor.equalsIgnoreCase("Valve")
      && (product.equalsIgnoreCase("Jupiter") || board.equalsIgnoreCase("Jupiter"));
#else
  return false;
#endif
}

}
