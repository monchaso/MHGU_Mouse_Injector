#include "Core/Memory.h"
#include "Core/MouseInput.h"
#include "Core/Scanner.h"
#include "Core/Unlocker.h"
#include <iostream>
#include <thread>

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
  if (!unlocker.Hook(addrXR)) {
    std::cerr << "Failed to Hook X-Right!" << std::endl;
    return 1;
  }

  // 2. NOP the others to disable fighting
  if (!unlocker.NopInstruction(addrXL))
    std::cerr << "Failed to NOP X-Left!" << std::endl;
  if (!unlocker.NopInstruction(addrYU))
    std::cerr << "Failed to NOP Y-Up!" << std::endl;
  if (!unlocker.NopInstruction(addrYD))
    std::cerr << "Failed to NOP Y-Down!" << std::endl;

  std::cout << "Hooks & Patches Applied!" << std::endl;

  MouseInput input;
  bool active = false;

  // Main Loop
  while (true) {
    // Toggle F3
    if (GetAsyncKeyState(VK_F3) & 1) {
      active = !active;
      std::cout << "Injector: " << (active ? "ENABLED" : "DISABLED")
                << std::endl;
      if (!active) {
        // unlocker.Restore(); // Optional: Restore original code when disabled?
      }
    }

    if (active) {
      uintptr_t pBase = unlocker.GetCameraBaseAddr();
      uintptr_t pOffset = unlocker.GetCameraOffsetAddr();

      // Read captured pointers
      uintptr_t baseAddr = mem.Read<uintptr_t>(pBase);
      uintptr_t offsetVal = mem.Read<uintptr_t>(pOffset);

      if (baseAddr != 0 && offsetVal != 0) {
        uintptr_t finalX = baseAddr + offsetVal;
        uintptr_t finalY = finalX - 4; // Y is usually 4 bytes before/after?
        // Based on previous analysis:
        // X: 20F10750F7C
        // Y: 20F10750F78 (4 bytes BEFORE X)

        // Read current angles
        float currentX = mem.Read<float>(finalX);
        float currentY = mem.Read<float>(finalY);

        // Get Mouse Delta
        float deltaX, deltaY;
        input.GetDelta(deltaX, deltaY);

        // Calculate New
        float newX, newY;
        input.CalculateNewAngles(currentX, currentY, deltaX, deltaY, newX,
                                 newY);

        // Write Back
        mem.Write<float>(finalX, newX);
        mem.Write<float>(finalY, newY);
      }
    }

    Sleep(10); // ~100Hz tick rate
  }

  return 0;
}
