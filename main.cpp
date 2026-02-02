#include "Core/Memory.h"
#include "Core/MouseInput.h"
#include "Core/Scanner.h"
#include "Core/Unlocker.h"
#include <iostream>

// MHGU Camera Instruction AOBs
// X Right: 89 04 2E 8B 44 24 6C ...
const std::string AOB_XR =
    "89 04 2E 8B 44 24 6C 8D 68 01 89 E8 89 44 24 6C 8B 44 24 70 8D 68 02";
// X Left:  89 04 2E 8B 44 24 64 ...
const std::string AOB_XL =
    "89 04 2E 8B 44 24 64 8D 68 01 89 E8 89 44 24 64 8B 44 24 68 8D 68 02";
// Y Up:    89 04 2E 8B 44 24 60 ...
const std::string AOB_YU =
    "89 04 2E 8B 44 24 60 8D 68 01 89 E8 89 44 24 60 8B 44 24 64 8D 68 02";
// Y Down:  89 04 2E 8B 44 24 68 ...
const std::string AOB_YD =
    "89 04 2E 8B 44 24 68 8D 68 01 89 E8 89 44 24 68 8B 44 24 6C 8D 68 02";

const std::string MASK = "xxxxxxxxxxxxxxxxxxxxxxx"; // No wildcards

Unlocker *globalUnlocker = nullptr;

struct EnumData {
  DWORD pid;
  HWND hWnd;
};

BOOL CALLBACK EnumProc(HWND hWnd, LPARAM lParam) {
  EnumData *data = (EnumData *)lParam;
  DWORD procId;
  GetWindowThreadProcessId(hWnd, &procId);
  // Check if process matches and window is visible (to avoid hidden windows)
  if (procId == data->pid && IsWindowVisible(hWnd)) {
    data->hWnd = hWnd;
    return FALSE; // Stop
  }
  return TRUE;
}

HWND GetWindowHandleFromPID(DWORD pid) {
  EnumData data = {pid, nullptr};
  EnumWindows(EnumProc, (LPARAM)&data);
  return data.hWnd;
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
    if (globalUnlocker) {
      std::cout << "\n[Exit] Restoring original game code..." << std::endl;
      globalUnlocker->Restore();
    }
  }
  return FALSE;
}

int main() {
  SetConsoleCtrlHandler(ConsoleHandler, TRUE);
  std::cout << "[MHGU Mouse Injector] Starting..." << std::endl;

  Memory mem;
  std::cout << "Waiting for Ryujinx.exe..." << std::endl;
  while (!mem.Attach(L"Ryujinx.exe")) {
    Sleep(1000);
  }
  std::cout << "Attached to Ryujinx (PID: " << mem.processID << ")"
            << std::endl;

  // Get Target Window (For Focus Safety)
  HWND hTargetWnd = GetWindowHandleFromPID(mem.processID);
  if (hTargetWnd) {
    std::cout << "Found Target Window HWND: " << hTargetWnd << std::endl;
  } else {
    std::cout
        << "[Warning] Could not find Ryujinx Window. Focus safety may fail."
        << std::endl;
  }

  Scanner scanner(&mem);

  uintptr_t addrXR = 0, addrXL = 0, addrYU = 0, addrYD = 0;

  std::cout << "Scanning for Camera Instructions..." << std::endl;
  while (addrXR == 0 || addrXL == 0 || addrYU == 0 || addrYD == 0) {
    if (addrXR == 0)
      addrXR = scanner.ScanPattern(AOB_XR, MASK);
    if (addrXL == 0)
      addrXL = scanner.ScanPattern(AOB_XL, MASK);
    if (addrYU == 0)
      addrYU = scanner.ScanPattern(AOB_YU, MASK);
    if (addrYD == 0)
      addrYD = scanner.ScanPattern(AOB_YD, MASK);

    if (addrXR == 0 || addrXL == 0 || addrYU == 0 || addrYD == 0) {
      std::cout << "Waiting for patterns... (XR:" << (bool)addrXR
                << " XL:" << (bool)addrXL << " YU:" << (bool)addrYU
                << " YD:" << (bool)addrYD << ")" << std::endl;
      Sleep(1000);
    }
  }

  std::cout << "Found All Patterns!" << std::endl;
  std::cout << "XR: " << std::hex << addrXR << std::endl;
  std::cout << "XL: " << std::hex << addrXL << std::endl;
  std::cout << "YU: " << std::hex << addrYU << std::endl;
  std::cout << "YD: " << std::hex << addrYD << std::endl;

  Unlocker unlocker(&mem);
  globalUnlocker = &unlocker;

  // 1. Hook X-Right (To Capture Pointers)
  auto applyPatches = [&]() {
    if (!unlocker.Hook(addrXR))
      std::cerr << "Failed to Hook X-Right!" << std::endl;
    if (!unlocker.NopInstruction(addrXL))
      std::cerr << "Failed to NOP X-Left!" << std::endl;
    if (!unlocker.NopInstruction(addrYU))
      std::cerr << "Failed to NOP Y-Up!" << std::endl;
    if (!unlocker.NopInstruction(addrYD))
      std::cerr << "Failed to NOP Y-Down!" << std::endl;
    std::cout << "Injector: ENABLED" << std::endl;
  };

  // Initial Apply
  applyPatches();

  MouseInput input;
  bool active = true;
  bool f3Pressed = false;

  // Load Config
  Settings currentSettings = LoadConfig("config.json");

  // Init Raw Input for Wheel
  if (!input.InitializeRawInput()) {
    std::cerr
        << "[Warning] Raw Input Init Failed. Wheel mapping may not work.\n";
  }

  std::cout << "\n[Controls]" << std::endl;
  std::cout << "  F3: Toggle Injector (Enabled/Disabled) [Reloads Config]"
            << std::endl;
  std::cout << "  END: Exit\n" << std::endl;

  while (true) {
    if (GetAsyncKeyState(VK_END) & 0x8000)
      break;

    // Toggle F3
    bool f3Now = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
    if (f3Now && !f3Pressed) {
      active = !active;
      if (active) {
        currentSettings = LoadConfig("config.json"); // Reload on Enable
        applyPatches();
      } else {
        unlocker.Restore();
        std::cout << "Injector: DISABLED" << std::endl;
      }
    }
    // Check Process Life (Auto-Exit)
    if (WaitForSingleObject(mem.hProcess, 0) == WAIT_OBJECT_0) {
      std::cout << "\n[Info] Target process exited. Closing..." << std::endl;
      active = false; // Don't try to restore memory
      break;
    }

    // Check Focus
    bool isFocused = (GetForegroundWindow() == hTargetWnd);
    // If we failed to find window, assume focused (fallback)
    if (!hTargetWnd)
      isFocused = true;

    // Always read monitor
    uintptr_t pBase = unlocker.GetCameraBaseAddr();
    uintptr_t pOffset = unlocker.GetCameraOffsetAddr();

    uintptr_t baseVal = 0, offsetVal = 0;
    baseVal = mem.Read<uintptr_t>(pBase);
    offsetVal = mem.Read<uintptr_t>(pOffset);

    if (baseVal != 0 && offsetVal != 0) {
      uintptr_t finalX = baseVal + offsetVal;
      uintptr_t finalY = finalX - 4;

      int16_t valX = mem.Read<int16_t>(finalX);
      int16_t valY = mem.Read<int16_t>(finalY);

      if (isFocused || !active) {
        printf("MONITOR [Active:%d] [Focus:%d] | X: %d (0x%04X) | Y: %d "
               "(0x%04X)      \r",
               active, isFocused, valX, (uint16_t)valX, valY, (uint16_t)valY);
      } else {
        printf("MONITOR [Active:%d] [PAUSED]   | X: %d (0x%04X) | Y: %d "
               "(0x%04X)      \r",
               active, valX, (uint16_t)valX, valY, (uint16_t)valY);
      }

      if (active && isFocused) {
        input.ProcessInput(currentSettings);    // Process Buttons & Key Mapping
        input.ProcessRawInput(currentSettings); // Process Wheel (Pump Messages)

        float deltaX, deltaY;
        input.GetDelta(deltaX, deltaY);

        if (deltaX != 0.0f || deltaY != 0.0f) {
          int16_t newX = 0, newY = 0;
          input.CalculateNewAngles(valX, valY, deltaX, deltaY, newX, newY,
                                   currentSettings);

          mem.Write<int16_t>(finalX, newX);
          mem.Write<int16_t>(finalY, newY);
        }
      }
    }
    Sleep(10);
  }

  // Ensure clean exit
  input.CleanupRawInput();
  if (active) {
    std::cout << "[Exit] Restoring original game code..." << std::endl;
    unlocker.Restore();
  }
  return 0;
}
