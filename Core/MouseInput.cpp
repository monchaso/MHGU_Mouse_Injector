#include "MouseInput.h"
#include <iostream>
#include <thread>
#include <windows.h>

// Helper window proc
LRESULT CALLBACK RawInputWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                 LPARAM lParam) {
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

MouseInput::MouseInput() {}

bool MouseInput::InitializeRawInput() {
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.lpfnWndProc = RawInputWndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = "MHGU_RawInput";
  RegisterClassEx(&wc);

  hMessageWindow = CreateWindowEx(0, wc.lpszClassName, NULL, 0, 0, 0, 0, 0,
                                  HWND_MESSAGE, NULL, wc.hInstance, NULL);
  if (!hMessageWindow)
    return false;

  RAWINPUTDEVICE rid;
  rid.usUsagePage = 0x01;        // Generic Desktop Controls
  rid.usUsage = 0x02;            // Mouse
  rid.dwFlags = RIDEV_INPUTSINK; // Receive input even when not in foreground
  rid.hwndTarget = hMessageWindow;

  if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
    std::cerr << "Failed to register raw input." << std::endl;
    return false;
  }
  return true;
}

void MouseInput::CleanupRawInput() {
  if (hMessageWindow) {
    DestroyWindow(hMessageWindow);
    hMessageWindow = nullptr;
  }
}

void MouseInput::ProcessRawInput(const Settings &settings) {
  MSG msg;
  while (PeekMessage(&msg, hMessageWindow, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_INPUT) {
      UINT dwSize = 0;
      GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, NULL, &dwSize,
                      sizeof(RAWINPUTHEADER));

      if (dwSize > 0) {
        // Static buffer to avoid alloc
        static uint8_t lpb[1024];
        if (GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, lpb, &dwSize,
                            sizeof(RAWINPUTHEADER)) == dwSize) {
          RAWINPUT *raw = (RAWINPUT *)lpb;
          if (raw->header.dwType == RIM_TYPEMOUSE) {
            if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
              short wheelDelta = (short)raw->data.mouse.usButtonData;
              int keyPress = 0;

              if (wheelDelta > 0 && settings.keyWheelUp > 0) {
                keyPress = settings.keyWheelUp;
              } else if (wheelDelta < 0 && settings.keyWheelDown > 0) {
                keyPress = settings.keyWheelDown;
              }

              if (keyPress > 0) {
                // Run in detached thread to avoid blocking main loop
                std::thread([this, keyPress]() {
                  SendKey(keyPress, true);
                  Sleep(50);
                  SendKey(keyPress, false);
                }).detach();
              }
            }
          }
        }
      }
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

// Basic update is replaced by ProcessInput with settings
void MouseInput::ProcessInput(const Settings &settings) {
  // Helper lambda to handle mapping
  auto HandleMapping = [&](int vKeyMouse, int vKeyBoard, bool &lastState) {
    if (vKeyBoard == 0)
      return; // No binding

    bool isDown = (GetAsyncKeyState(vKeyMouse) & 0x8000) != 0;
    if (isDown != lastState) {
      SendKey(vKeyBoard, isDown);
      lastState = isDown;
    }
  };

  HandleMapping(VK_LBUTTON, settings.keyLMB, lastLMB);
  HandleMapping(VK_RBUTTON, settings.keyRMB, lastRMB);
  HandleMapping(VK_MBUTTON, settings.keyMMB, lastMMB);
  HandleMapping(VK_XBUTTON1, settings.keyX1, lastX1);
  HandleMapping(VK_XBUTTON2, settings.keyX2, lastX2);
}

void MouseInput::SendKey(int vk, bool down) {
  INPUT input = {0};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
  SendInput(1, &input, sizeof(INPUT));
}

void MouseInput::GetDelta(float &x, float &y) {
  POINT p;
  GetCursorPos(&p);

  // Assume center is 960x540 (1920x1080 / 2)
  // Or get dynamically from SystemMetrics
  int centerX = 1920 / 2;
  int centerY = 1080 / 2;

  x = (float)(p.x - centerX);
  y = (float)(p.y - centerY);

  // Reset cursor to center
  SetCursorPos(centerX, centerY);
}

// Use int16_t for I/O, but float/int for logic
void MouseInput::CalculateNewAngles(int16_t currentX, int16_t currentY,
                                    float deltaX, float deltaY, int16_t &outX,
                                    int16_t &outY, const Settings &settings) {
  // Base Sensitivity (Negative was correct for user)
  // If Invert is TRUE, we flip sign.
  // Default (False) = -1.0 * Sensitivity
  float multX = settings.invertX ? 1.0f : -1.0f;
  float multY = settings.invertY ? 1.0f : -1.0f;

  // Config Sensitivity acts as multiplier? Or raw value?
  // Let's assume the config value IS the speed.
  // If config says 25.0, we use 25.0 * mult.
  const float SENSITIVITY_X = settings.sensitivityX * multX;
  const float SENSITIVITY_Y = settings.sensitivityY * multY;

  // --- X Axis (Yaw) ---
  // wrapping 0 - 65535
  // Cast to int32 to avoid overflow during add
  // Bitwise AND with 0xFFFF handles both for standard unsigned behavior,
  // but since we treat it as int16_t (signed in C++ usually), we need to be
  // careful with negative casts. Easier to treat as "Unsigned 16-bit" logic:
  uint16_t uX = (uint16_t)currentX;
  int32_t uNextX = (int32_t)uX + (int32_t)(deltaX * SENSITIVITY_X);
  outX = (int16_t)(uNextX); // Implicitly wraps due to cast

  // --- Y Axis (Pitch) ---
  // Clamping -12000 to +10000 (User Estimates)
  // Down = Positive (10240), Up = Negative (-12032/53504)
  // Mouse Down (Positive DeltaY) -> Look Down (Increase Angle)
  int32_t nextY = (int32_t)currentY + (int32_t)(deltaY * SENSITIVITY_Y);

  // Clamp
  const int16_t MIN_PITCH = -12000;
  const int16_t MAX_PITCH = 10000;

  if (nextY > MAX_PITCH)
    nextY = MAX_PITCH;
  if (nextY < MIN_PITCH)
    nextY = MIN_PITCH;

  outY = (int16_t)nextY;
}
