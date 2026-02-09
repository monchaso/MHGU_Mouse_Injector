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

  // Initialize Core Components
  Scanner scanner(&mem);
  Unlocker unlocker(&mem);
  globalUnlocker = &unlocker;

  MouseInput input;
  // Load Config
  Settings currentSettings = LoadConfig("config.json");
  // Init Raw Input for Wheel
  if (!input.InitializeRawInput()) {
    std::cerr
        << "[Warning] Raw Input Init Failed. Wheel mapping may not work.\n";
  }

  // Scanning State
  uintptr_t addrXR = 0, addrXL = 0, addrYU = 0, addrYD = 0;
  bool addressesFound = false;
  DWORD lastScanTime = 0;

  // Injection State
  bool active = true;
  bool f3Pressed = false;

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

  std::cout << "\n[Controls]" << std::endl;
  std::cout << "  F3: Toggle Injector (Enabled/Disabled) [Reloads Config]"
            << std::endl;
  std::cout << "  END: Exit\n" << std::endl;
  std::cout << "Scanning for Camera Instructions (background)..." << std::endl;

  while (true) {
    if (GetAsyncKeyState(VK_END) & 0x8000)
      break;

    // Toggle F3
    bool f3Now = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
    if (f3Now && !f3Pressed) {
      active = !active;
      if (active) {
        currentSettings = LoadConfig("config.json"); // Reload on Enable
        if (addressesFound) {
          applyPatches();
        }
      } else {
        if (addressesFound) {
          unlocker.Restore();
        }
        std::cout << "Injector: DISABLED" << std::endl;
      }
    }
    f3Pressed = f3Now;

    // Check Process Life (Auto-Exit)
    DWORD exitCode = 0;
    if (WaitForSingleObject(mem.hProcess, 0) == WAIT_OBJECT_0 ||
        (GetExitCodeProcess(mem.hProcess, &exitCode) &&
         exitCode != STILL_ACTIVE)) {
      std::cout << "\n[Info] Target process exited. Closing..." << std::endl;
      active = false; // Don't try to restore memory
      break;
    }

    // Periodic Tasks (Scanning & Window Check) - Every 1 second
    if (GetTickCount() - lastScanTime > 1000) {
      lastScanTime = GetTickCount();

      // Retry Window Handle if missing
      if (!hTargetWnd) {
        hTargetWnd = GetWindowHandleFromPID(mem.processID);
        if (hTargetWnd) {
          std::cout << "\n[Info] Target Window Found: " << hTargetWnd
                    << std::endl;
        }
      }

      // Memory Scanning
      if (!addressesFound) {
        if (addrXR == 0)
          addrXR = scanner.ScanPattern(AOB_XR, MASK);
        if (addrXL == 0)
          addrXL = scanner.ScanPattern(AOB_XL, MASK);
        if (addrYU == 0)
          addrYU = scanner.ScanPattern(AOB_YU, MASK);
        if (addrYD == 0)
          addrYD = scanner.ScanPattern(AOB_YD, MASK);

        if (addrXR != 0 && addrXL != 0 && addrYU != 0 && addrYD != 0) {
          addressesFound = true;
          std::cout << "Found All Patterns!" << std::endl;
          std::cout << "XR: " << std::hex << addrXR << std::endl;
          std::cout << "XL: " << std::hex << addrXL << std::endl;
          std::cout << "YU: " << std::hex << addrYU << std::endl;
          std::cout << "YD: " << std::hex << addrYD << std::endl;

          // Apply patches immediately if active
          if (active) {
            applyPatches();
          }
        }
      }
    }

    // Check Focus
    bool isFocused = (GetForegroundWindow() == hTargetWnd);
    if (!hTargetWnd)
      isFocused = true; // Fallback to safe-assume if window never found

    // Input Processing (ALWAYS RUNS if Active & Focused)
    // Allows buttons to work even if addresses are not found yet (e.g. Loading
    // Screen)
    if (active && isFocused) {
      input.ProcessInput(currentSettings);    // Process Buttons & Key Mapping
      input.ProcessRawInput(currentSettings); // Process Wheel
    }

    if (addressesFound) {
      // Read Memory Values for Monitor
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

        if (active && isFocused) {
          printf("MONITOR [Active:1] [Focus:1] | X: %d (0x%04X) | Y: %d "
                 "(0x%04X)      \r",
                 valX, (uint16_t)valX, valY, (uint16_t)valY);

          // Injection Logic
          float deltaX, deltaY;
          input.GetDelta(deltaX, deltaY);

          if (deltaX != 0.0f || deltaY != 0.0f) {
            int16_t newX = 0, newY = 0;
            input.CalculateNewAngles(valX, valY, deltaX, deltaY, newX, newY,
                                     currentSettings);
            mem.Write<int16_t>(finalX, newX);
            mem.Write<int16_t>(finalY, newY);
          }
        } else {
          // Paused / Not Focused
          printf("MONITOR [Active:%d] [Focus:%d] | X: %d (0x%04X) | Y: %d "
                 "(0x%04X)      \r",
                 active, isFocused, valX, (uint16_t)valX, valY, (uint16_t)valY);
        }
      }
    } else {
      // Scanning
      printf("MONITOR [Active:%d] [Found:0] | Scanning...                      "
             "   \r",
             active);
    }

    Sleep(10);
  }

  // Ensure clean exit
  input.CleanupRawInput();
  if (active && addressesFound) {
    std::cout << "\n[Exit] Restoring original game code..." << std::endl;
    unlocker.Restore();
  }
  return 0;
}
