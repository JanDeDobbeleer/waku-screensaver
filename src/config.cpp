#include "config.h"

#include <algorithm>
#include <fstream>

#include "animation/animation.h"

namespace waku::screensaver {
namespace {

std::string_view trim(std::string_view value) {
  const auto start = value.find_first_not_of(" \t\r");
  if (start == value.npos) return {};
  return value.substr(start, value.find_last_not_of(" \t\r") - start + 1);
}

bool setWord(Config& config, std::string_view value) {
  if (value.empty() || value.size() > 32) return false;
  std::string word(value);
  for (char& character : word) {
    if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
    if (character != ' ' && !(character >= 'A' && character <= 'Z') &&
        !(character >= '0' && character <= '9'))
      return false;
  }
  config.word = word;
  return true;
}

bool setEffects(Config& config, std::string_view value) {
  std::vector<unsigned> effects;
  do {
    const auto comma = value.find(',');
    const auto name = trim(value.substr(0, comma));
    const auto found =
        std::find(kEffectNames.begin(), kEffectNames.end(), std::wstring(name.begin(), name.end()));
    if (found == kEffectNames.end()) return false;
    const auto effect = static_cast<unsigned>(found - kEffectNames.begin());
    if (std::find(effects.begin(), effects.end(), effect) == effects.end())
      effects.push_back(effect);
    if (comma == value.npos) break;
    value.remove_prefix(comma + 1);
  } while (true);
  config.effects = std::move(effects);
  return true;
}

}  // namespace

Config parseConfig(std::string_view contents) {
  Config config;
  if (contents.starts_with("\xef\xbb\xbf")) contents.remove_prefix(3);
  bool screensaver = false;
  while (!contents.empty()) {
    const auto newline = contents.find('\n');
    const auto line = trim(contents.substr(0, newline));
    contents = newline == contents.npos ? std::string_view{} : contents.substr(newline + 1);
    if (line.empty() || line[0] == ';' || line[0] == '#') continue;
    if (line[0] == '[') {
      screensaver = line == "[screensaver]";
      continue;
    }
    if (!screensaver) continue;
    const auto equals = line.find('=');
    if (equals == line.npos) continue;
    const auto key = trim(line.substr(0, equals));
    const auto value = trim(line.substr(equals + 1));
    if (key == "word") setWord(config, value);
    if (key == "animations") setEffects(config, value);
  }
  return config;
}

Config loadConfig(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  // Configuration is small; bound reads even if the path points at an unrelated file.
  std::string contents(16385, '\0');
  input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
  if (input.bad() || input.gcount() > 16384) return {};
  contents.resize(static_cast<size_t>(input.gcount()));
  return parseConfig(contents);
}

}  // namespace waku::screensaver
