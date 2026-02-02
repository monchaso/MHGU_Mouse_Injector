#pragma once
#include "Memory.h"
#include <string>
#include <vector>

class Scanner {
public:
  Memory *mem;

  Scanner(Memory *memory);

  // Scans all executable regions for the pattern
  uintptr_t ScanPattern(const std::string &pattern, const std::string &mask);

  // Naive scan (fallback)
  uintptr_t ScanBasic(const std::vector<uint8_t> &buffer,
                      const std::vector<uint8_t> &patternBytes,
                      const std::string &mask);

  // SIMD scan (AVX2)
  uintptr_t ScanSIMD(const std::vector<uint8_t> &buffer,
                     const std::vector<uint8_t> &patternBytes,
                     const std::string &mask);
};
