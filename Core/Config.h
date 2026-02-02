#pragma once
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Settings {
  bool invertX = false;
  bool invertY = false;
  float sensitivityX = 25.0f;
  float sensitivityY = 25.0f;

  // Key bindings (Vector for multi-key support)
  std::vector<int> keysLMB;
  std::vector<int> keysRMB;
  std::vector<int> keysMMB;
  std::vector<int> keysX1;
  std::vector<int> keysX2;

  // Wheel remains single usage for now (since logic is precise)
  int keyWheelUp = 0;
  int keyWheelDown = 0;
  int keyMHHold = 0;
};

#include <map>

// Helper to map strings to VK codes
inline int GetVkFromString(std::string key) {
  // Normalize to uppercase
  std::transform(key.begin(), key.end(), key.begin(), ::toupper);

  static std::map<std::string, int> keyMap = {
      {"VK_LBUTTON", 0x01},  {"LBUTTON", 0x01},    {"VK_RBUTTON", 0x02},
      {"RBUTTON", 0x02},     {"VK_MBUTTON", 0x04}, {"MBUTTON", 0x04},
      {"VK_XBUTTON1", 0x05}, {"XBUTTON1", 0x05},   {"VK_XBUTTON2", 0x06},
      {"XBUTTON2", 0x06},    {"VK_BACK", 0x08},    {"BACKSPACE", 0x08},
      {"VK_TAB", 0x09},      {"TAB", 0x09},        {"VK_RETURN", 0x0D},
      {"ENTER", 0x0D},       {"RETURN", 0x0D},     {"VK_SHIFT", 0x10},
      {"SHIFT", 0x10},       {"VK_CONTROL", 0x11}, {"CONTROL", 0x11},
      {"CTRL", 0x11},        {"VK_MENU", 0x12},    {"ALT", 0x12},
      {"VK_PAUSE", 0x13},    {"PAUSE", 0x13},      {"VK_CAPITAL", 0x14},
      {"CAPS", 0x14},        {"CAPSLOCK", 0x14},   {"VK_ESCAPE", 0x1B},
      {"ESC", 0x1B},         {"VK_SPACE", 0x20},   {"SPACE", 0x20},
      {"VK_PRIOR", 0x21},    {"PAGEUP", 0x21},     {"VK_NEXT", 0x22},
      {"PAGEDOWN", 0x22},    {"VK_END", 0x23},     {"END", 0x23},
      {"VK_HOME", 0x24},     {"HOME", 0x24},       {"VK_LEFT", 0x25},
      {"LEFT", 0x25},        {"VK_UP", 0x26},      {"UP", 0x26},
      {"VK_RIGHT", 0x27},    {"RIGHT", 0x27},      {"VK_DOWN", 0x28},
      {"DOWN", 0x28},        {"VK_INSERT", 0x2D},  {"INSERT", 0x2D},
      {"VK_DELETE", 0x2E},   {"DELETE", 0x2E},     {"VK_LWIN", 0x5B},
      {"LWIN", 0x5B},        {"VK_RWIN", 0x5C},    {"RWIN", 0x5C},
      {"VK_NUMPAD0", 0x60},  {"NUMPAD0", 0x60},    {"VK_NUMPAD1", 0x61},
      {"NUMPAD1", 0x61},     {"VK_F1", 0x70},      {"F1", 0x70},
      {"VK_F2", 0x71},       {"F2", 0x71},         {"VK_F3", 0x72},
      {"F3", 0x72},          {"VK_F4", 0x73},      {"F4", 0x73},
      {"VK_F5", 0x74},       {"F5", 0x74},         {"VK_F6", 0x75},
      {"F6", 0x75},          {"VK_F7", 0x76},      {"F7", 0x76},
      {"VK_F8", 0x77},       {"F8", 0x77},         {"VK_F9", 0x78},
      {"F9", 0x78},          {"VK_F10", 0x79},     {"F10", 0x79},
      {"VK_F11", 0x7A},      {"F11", 0x7A},        {"VK_F12", 0x7B},
      {"F12", 0x7B},         {"VK_LSHIFT", 0xA0},  {"LSHIFT", 0xA0},
      {"VK_RSHIFT", 0xA1},   {"RSHIFT", 0xA1},     {"VK_LCONTROL", 0xA2},
      {"LCONTROL", 0xA2},    {"LCTRL", 0xA2},      {"VK_RCONTROL", 0xA3},
      {"RCONTROL", 0xA3},    {"RCTRL", 0xA3},      {"VK_LMENU", 0xA4},
      {"LALT", 0xA4},        {"VK_RMENU", 0xA5},   {"RALT", 0xA5}};

  // Single characters (0-9, A-Z)
  if (key.length() == 1) {
    char c = key[0];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))
      return (int)c;
  }

  if (keyMap.count(key))
    return keyMap[key];

  // Fallback: Check if it's "VK_X" where X is a single char
  if (key.length() == 4 && key.substr(0, 3) == "VK_") {
    return (int)key[3];
  }

  return 0;
}

// Helper: Parse "[\"VK_A\", \"VK_B\"]" or "\"VK_A\""
inline std::vector<int> ParseIntArray(std::string val) {
  std::vector<int> out;
  if (val.empty())
    return out;

  // Remove brackets
  val.erase(std::remove(val.begin(), val.end(), '['), val.end());
  val.erase(std::remove(val.begin(), val.end(), ']'), val.end());

  std::stringstream ss(val);
  std::string segment;
  while (std::getline(ss, segment, ',')) {
    try {
      if (!segment.empty()) {
        // Check for quotes indicating string
        size_t q1 = segment.find('\"');
        size_t q2 = segment.find('\"', q1 + 1);

        if (q1 != std::string::npos && q2 != std::string::npos) {
          // Extract string content
          std::string strKey = segment.substr(q1 + 1, q2 - q1 - 1);

          // Handle Empty String as NULL (No-Op)
          if (strKey.empty())
            continue;

          int vk = GetVkFromString(strKey);
          if (vk > 0)
            out.push_back(vk);
        } else {
          // Ignoring raw integers as per request
        }
      }
    } catch (...) {
    }
  }
  return out;
}

// Robust manual JSON parser (Handles multi-line arrays)
inline Settings LoadConfig(const std::string &path) {
  Settings s;
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "[Config] Not found, using defaults.\n";
    return s;
  }

  // Read entire file
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  // Remove ALL whitespace from content
  content.erase(std::remove_if(content.begin(), content.end(), ::isspace),
                content.end());

  // Helper to find value for key
  auto GetVal = [&](const std::string &keyName) -> std::string {
    std::string search = "\"" + keyName + "\":";
    size_t pos = content.find(search);
    if (pos == std::string::npos)
      return "";

    size_t start = pos + search.length();
    size_t end = start;

    // If array start
    if (content[start] == '[') {
      end = content.find(']', start);
      if (end != std::string::npos)
        end++; // Include ']'
    } else {
      // Find comma or brace
      size_t endComma = content.find(',', start);
      size_t endBrace = content.find('}', start);
      if (endComma == std::string::npos)
        end = endBrace;
      else if (endBrace == std::string::npos)
        end = endComma;
      else
        end = (std::min)(endComma, endBrace);
    }

    if (end == std::string::npos)
      return "";
    return content.substr(start, end - start);
  };

  try {
    std::string v;

    if (!(v = GetVal("invert_x")).empty())
      s.invertX = (v == "true");
    if (!(v = GetVal("invert_y")).empty())
      s.invertY = (v == "true");
    if (!(v = GetVal("sensitivity_x")).empty())
      s.sensitivityX = std::stof(v);
    if (!(v = GetVal("sensitivity_y")).empty())
      s.sensitivityY = std::stof(v);

    if (!(v = GetVal("key_lmb")).empty())
      s.keysLMB = ParseIntArray(v);
    if (!(v = GetVal("key_rmb")).empty())
      s.keysRMB = ParseIntArray(v);
    if (!(v = GetVal("key_mmb")).empty())
      s.keysMMB = ParseIntArray(v);
    if (!(v = GetVal("key_x1")).empty())
      s.keysX1 = ParseIntArray(v);
    if (!(v = GetVal("key_x2")).empty())
      s.keysX2 = ParseIntArray(v);

    if (!(v = GetVal("key_mw_up")).empty()) {
      std::vector<int> k = ParseIntArray(v);
      if (!k.empty())
        s.keyWheelUp = k[0];
    }
    if (!(v = GetVal("key_mw_down")).empty()) {
      std::vector<int> k = ParseIntArray(v);
      if (!k.empty())
        s.keyWheelDown = k[0];
    }

    if (!(v = GetVal("key_mh_hold")).empty()) {
      std::vector<int> k = ParseIntArray(v);
      if (!k.empty())
        s.keyMHHold = k[0];
    }

  } catch (...) {
    std::cerr << "[Config] Parse Error\n";
  }

  std::cout << "[Config] Loaded!\n";
  return s;
}
