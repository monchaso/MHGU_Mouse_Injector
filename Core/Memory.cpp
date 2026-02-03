#include "Memory.h"
#include <tlhelp32.h>

Memory::Memory() {}

Memory::~Memory() {
  if (hProcess)
    CloseHandle(hProcess);
}

bool Memory::Attach(const std::wstring &processName) {
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap == INVALID_HANDLE_VALUE)
    return false;

  PROCESSENTRY32W pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32W);

  if (Process32FirstW(hSnap, &pe32)) {
    do {
      if (processName == pe32.szExeFile) {
        processID = pe32.th32ProcessID;
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
        break;
      }
    } while (Process32NextW(hSnap, &pe32));
  }
  CloseHandle(hSnap);
  return hProcess != NULL;
}

bool Memory::IsOpen() { return hProcess != NULL; }

bool Memory::ReadBuffer(uintptr_t address, void *buffer, size_t size) {
  SIZE_T bytesRead;
  return ReadProcessMemory(hProcess, (LPCVOID)address, buffer, size,
                           &bytesRead);
}

bool Memory::WriteBuffer(uintptr_t address, const void *buffer, size_t size) {
  DWORD oldProtect;
  if (!VirtualProtectEx(hProcess, (LPVOID)address, size, PAGE_EXECUTE_READWRITE,
                        &oldProtect)) {
    return false;
  }

  SIZE_T bytesWritten;
  bool result = WriteProcessMemory(hProcess, (LPVOID)address, buffer, size,
                                   &bytesWritten);

  DWORD temp;
  VirtualProtectEx(hProcess, (LPVOID)address, size, oldProtect, &temp);

  return result;
}

std::vector<MEMORY_BASIC_INFORMATION> Memory::GetExecutableRegions() {
  std::vector<MEMORY_BASIC_INFORMATION> regions;
  uintptr_t address = 0;
  MEMORY_BASIC_INFORMATION mbi;

  while (VirtualQueryEx(hProcess, (LPCVOID)address, &mbi, sizeof(mbi))) {
    if (mbi.State == MEM_COMMIT &&
        (mbi.Protect == PAGE_EXECUTE || mbi.Protect == PAGE_EXECUTE_READ ||
         mbi.Protect == PAGE_EXECUTE_READWRITE)) {
      regions.push_back(mbi);
    }
    address = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
  }
  return regions;
}

uintptr_t Memory::Allocate(size_t size) {
  return (uintptr_t)VirtualAllocEx(
      hProcess, NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}
