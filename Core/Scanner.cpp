#include "Scanner.h"
#include <immintrin.h>
#include <sstream>

Scanner::Scanner(Memory *memory) : mem(memory) {}

std::vector<uint8_t> ParsePattern(const std::string &pattern) {
  std::vector<uint8_t> bytes;
  std::stringstream ss(pattern);
  std::string byteStr;
  while (ss >> byteStr) {
    if (byteStr == "??" || byteStr == "?")
      bytes.push_back(0);
    else
      bytes.push_back((uint8_t)std::stoul(byteStr, nullptr, 16));
  }
  return bytes;
}

uintptr_t Scanner::ScanPattern(const std::string &pattern,
                               const std::string &mask) {
  auto regions = mem->GetExecutableRegions();
  auto patternBytes = ParsePattern(pattern);

  for (const auto &region : regions) {
    size_t size = region.RegionSize;
    std::vector<uint8_t> buffer(size);

    if (mem->ReadBuffer((uintptr_t)region.BaseAddress, buffer.data(), size)) {
      // Check if SIMD is viable (simple check) or use basic
      // For now, defaulting to Basic scan for compatibility, but designated for
      // SIMD upgrade
      uintptr_t offset = ScanBasic(buffer, patternBytes, mask);
      if (offset != -1) {
        return (uintptr_t)region.BaseAddress + offset;
      }
    }
  }
  return 0;
}

uintptr_t Scanner::ScanBasic(const std::vector<uint8_t> &buffer,
                             const std::vector<uint8_t> &patternBytes,
                             const std::string &mask) {
  size_t scanLength = buffer.size() - patternBytes.size();
  for (size_t i = 0; i < scanLength; ++i) {
    bool found = true;
    for (size_t j = 0; j < patternBytes.size(); ++j) {
      if (mask[j] != '?' && buffer[i + j] != patternBytes[j]) {
        found = false;
        break;
      }
    }
    if (found)
      return i;
  }
  return -1;
}

// SIMD Placeholder - Requires -mavx2 or /arch:AVX2
uintptr_t Scanner::ScanSIMD(const std::vector<uint8_t> &buffer,
                            const std::vector<uint8_t> &patternBytes,
                            const std::string &mask) {
  // Implementation detail: Load pattern into __m256i, iterate buffer with
  // _mm256_loadu_si256 Compare with _mm256_cmpeq_epi8, check mask with
  // _mm256_movemask_epi8 Return index if found
  return ScanBasic(
      buffer, patternBytes,
      mask); // Fallback for now to ensure compilation without AVX flags setup
}
