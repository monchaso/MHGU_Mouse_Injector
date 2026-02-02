#pragma once
#include "Memory.h"
#include <vector> // Added for std::vector

class Unlocker {
  Memory *mem;
  uintptr_t targetAddress = 0;
  uintptr_t trampolineAddress = 0;

  // Addresses for our captured registers
  uintptr_t pCameraBase = 0;
  uintptr_t pCameraOffset = 0;

  struct Patch {
    uintptr_t address;
    std::vector<uint8_t> originalBytes;
  };
  std::vector<Patch> patches;

public:
  Unlocker(Memory *memory);

  // Creates the hook/trampoline at the target address
  bool Hook(uintptr_t address);

  // NOPs the instruction (3 bytes) to stop game writing
  bool NopInstruction(uintptr_t address);

  // Restores original bytes (optional cleanup)
  void Restore();

  // Getters for the captured pointers
  uintptr_t GetCameraBaseAddr();
  uintptr_t GetCameraOffsetAddr();
};
