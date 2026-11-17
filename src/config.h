#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace waku::screensaver {

struct Config {
  std::string word = "WAKU";
  std::vector<unsigned> effects{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
};

inline constexpr std::string_view kDefaultConfig =
    "[screensaver]\n"
    "; Letters A-Z, digits 0-9, and spaces; up to 32 characters.\n"
    "word=WAKU\n"
    "; Effects run in this order. Chase runs longer to make dot eating visible.\n"
    "animations=rain,decrypt,waves,beams,matrix,burn,blocks,blackhole,starfield,retro-stars,"
    "parallax,chase\n";

Config parseConfig(std::string_view contents);
Config loadConfig(const std::filesystem::path& path);

}  // namespace waku::screensaver
