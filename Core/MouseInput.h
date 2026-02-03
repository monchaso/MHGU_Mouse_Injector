#pragma once
#include "Config.h"
#include <cstdint>
#include <windows.h>

class MouseInput {
public:
  MouseInput();

  // Processes mouse movement and buttons based on settings
  void ProcessInput(const Settings &settings);

  // Raw Input for Wheel
  bool InitializeRawInput();
  void ProcessRawInput(const Settings &settings);
  void CleanupRawInput();

  // Returns the delta movement since last update
  void GetDelta(float &x, float &y);

  // Core Math
  // currentX, currentY: Raw game values (int16_t)
  // sensitivity: multiplier (approx 20.0f)
  // outX, outY: new values to flip/write
  void CalculateNewAngles(int16_t currentX, int16_t currentY, float deltaX,
                          float deltaY, int16_t &outX, int16_t &outY,
                          const Settings &settings);

private:
  void SendKey(int vk, bool down);
  // Track button states to avoid spamming "KeyDown"
  HWND hMessageWindow = nullptr;
  bool lastLMB = false;
  bool lastRMB = false;
  bool lastMMB = false;
  bool lastX1 = false;
  bool lastX2 = false;
};
