#pragma once
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

struct Settings {
  bool invertX = false;
  bool invertY = false;
  float sensitivityX = 25.0f;
  float sensitivityY = 25.0f;

  // Key bindings (Virtual Key Codes)
  int keyLMB = 0;
  int keyRMB = 0;
  int keyMMB = 0;
  int keyX1 = 0;
  int keyX2 = 0;
  int keyWheelUp = 0;
  int keyWheelDown = 0;
};

// Simple manual JSON parser (Key: Value)
inline Settings LoadConfig(const std::string &path) {
  Settings s;
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "[Config] Not found, using defaults.\n";
    return s;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Remove whitespace
    line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

    // Skip brackets/empty
    if (line.empty() || line == "{" || line == "}")
      continue;

    // Parse "key":value
    size_t colon = line.find(':');
    if (colon == std::string::npos)
      continue;

    std::string key = line.substr(0, colon);
    std::string val = line.substr(colon + 1);

    // Remove quotes from key
    key.erase(std::remove(key.begin(), key.end(), '\"'), key.end());
    // Remove trailing comma from value if present
    if (!val.empty() && val.back() == ',')
      val.pop_back();

    // Assign
    try {
      if (key == "invert_x")
        s.invertX = (val == "true");
      else if (key == "invert_y")
        s.invertY = (val == "true");
      else if (key == "sensitivity_x")
        s.sensitivityX = std::stof(val);
      else if (key == "sensitivity_y")
        s.sensitivityY = std::stof(val);
      else if (key == "key_lmb")
        s.keyLMB = std::stoi(val);
      else if (key == "key_rmb")
        s.keyRMB = std::stoi(val);
      else if (key == "key_mmb")
        s.keyMMB = std::stoi(val);
      else if (key == "key_x1")
        s.keyX1 = std::stoi(val);
      else if (key == "key_x2")
        s.keyX2 = std::stoi(val);
      else if (key == "key_mw_up")
        s.keyWheelUp = std::stoi(val);
      else if (key == "key_mw_down")
        s.keyWheelDown = std::stoi(val);
    } catch (...) {
      // Ignore parse errors
    }
  }
  std::cout << "[Config] Loaded!\n";
  return s;
}
