#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>


class Memory {
public:
  HANDLE hProcess = NULL;
  DWORD processID = 0;

  Memory();
  ~Memory();

  bool Attach(const std::wstring &processName);
  bool IsOpen();

  // Generic Read/Write
  template <typename T> T Read(uintptr_t address) {
    T value;
    SIZE_T bytesRead;
    ReadProcessMemory(hProcess, (LPCVOID)address, &value, sizeof(T),
                      &bytesRead);
    return value;
  }

  template <typename T> void Write(uintptr_t address, T value) {
    SIZE_T bytesWritten;
    WriteProcessMemory(hProcess, (LPVOID)address, &value, sizeof(T),
                       &bytesWritten);
  }

  // Raw Buffer Helpers
  bool ReadBuffer(uintptr_t address, void *buffer, size_t size);
  bool WriteBuffer(uintptr_t address, const void *buffer, size_t size);

  // Memory Info
  std::vector<MEMORY_BASIC_INFORMATION> GetExecutableRegions();

  // trampoline/alloc
  uintptr_t Allocate(size_t size);
};
