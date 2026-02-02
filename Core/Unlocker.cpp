#include "Unlocker.h"
#include <iostream>

// Standard 14-byte Absolute JMP (x64)
// FF 25 00 00 00 00 [Low addr] [High addr]
// Standard 14-byte Absolute JMP (x64)
// FF 25 00 00 00 00 [Low addr] [High addr]

Unlocker::Unlocker(Memory *memory) : mem(memory) {}

bool Unlocker::Hook(uintptr_t address) {
  if (trampolineAddress != 0)
    return true; // Already hooked
  targetAddress = address;

  // 1. Calculate how many bytes we need to steal.
  // We need 14 bytes for the JMP.
  // Instructions at target (from analysis):
  // 1: 89 04 2E       (3 bytes) mov [rsi+rbp],eax
  // 2: 8B 44 24 6C    (4 bytes) mov eax,[rsp+6C]
  // 3: 8D 68 01       (3 bytes) lea ebp,[rax+01]
  // 4: 89 E8          (2 bytes) mov eax, ebp
  // 5: 89 44 24 6C    (4 bytes) mov [rsp+6C], eax
  // Total: 16 bytes (perfect, > 14)
  const size_t STOLEN_SIZE = 16;

  // 2. Allocate Trampoline Memory
  trampolineAddress = mem->Allocate(1024); // 1KB RWX
  if (!trampolineAddress) {
    std::cerr << "[Unlocker] Failed to allocate trampoline!" << std::endl;
    return false;
  }

  // Define storage locations (at end of page for safety)
  pCameraBase = trampolineAddress + 0x300;
  pCameraOffset = trampolineAddress + 0x308;
  // Initial values 0
  mem->Write<uintptr_t>(pCameraBase, 0);
  mem->Write<uintptr_t>(pCameraOffset, 0);

  // 3. Construct Shellcode
  std::vector<uint8_t> shellcode;

  // -- Backup registers we clobber? (RAX is safe? No, we use RAX/R11 for
  // scratch usually) Actually, we can just use register-to-memory moves. 'mov
  // [mem], reg' doesn't clobber. mov [REL_ADDR], RSI x64 absolute addressing is
  // tricky. Best to use RAX as scratch if we can preserve it. Or use
  // RIP-relative if close? No, remote alloc might be far. Let's use:
  //   push rax
  //   mov rax, ABS_ADDR
  //   mov [rax], rsi
  //   mov rax, ABS_ADDR2
  //   mov [rax], rbp
  //   pop rax

  // PUSH RAX (50)
  shellcode.push_back(0x50);

  // MOV RAX, pCameraBase (48 B8 + 8 bytes)
  shellcode.push_back(0x48);
  shellcode.push_back(0xB8);
  uintptr_t *ptrBase = (uintptr_t *)&pCameraBase;
  for (int i = 0; i < 8; i++)
    shellcode.push_back(((uint8_t *)ptrBase)[i]);

  // MOV [RAX], RSI (48 89 30)
  shellcode.push_back(0x48);
  shellcode.push_back(0x89);
  shellcode.push_back(0x30);

  // MOV RAX, pCameraOffset (48 B8 + 8 bytes)
  shellcode.push_back(0x48);
  shellcode.push_back(0xB8);
  uintptr_t *ptrOffset = (uintptr_t *)&pCameraOffset;
  for (int i = 0; i < 8; i++)
    shellcode.push_back(((uint8_t *)ptrOffset)[i]);

  // MOV [RAX], RBP (48 89 28)
  shellcode.push_back(0x48);
  shellcode.push_back(0x89);
  shellcode.push_back(0x28);

  // POP RAX (58)
  shellcode.push_back(0x58);

  // -- Original Stolen Instructions (16 bytes) --
  // We read them from the process to be 100% sure we match exact verified bytes
  std::vector<uint8_t> originalBytes(STOLEN_SIZE);
  mem->ReadBuffer(targetAddress, originalBytes.data(), STOLEN_SIZE);

  // Append original bytes to shellcode
  // IMPORTANT: We want to DISABLE the original camera write instruction (first
  // 3 bytes: 89 04 2E) so the game doesn't fight our injection. Bytes 0-2: mov
  // [rsi+rbp],eax  -> NOP these! Bytes 3-15: Other instructions -> Keep these!

  for (size_t i = 0; i < originalBytes.size(); ++i) {
    if (i < 3)
      shellcode.push_back(0x90); // NOP the write
    else
      shellcode.push_back(originalBytes[i]);
  }

  // -- JMP Back to Target + 16 --
  // FF 25 00 00 00 00 [Address]
  uintptr_t returnAddress = targetAddress + STOLEN_SIZE;
  shellcode.push_back(0xFF);
  shellcode.push_back(0x25);
  shellcode.push_back(0x00);
  shellcode.push_back(0x00);
  shellcode.push_back(0x00);
  shellcode.push_back(0x00);
  uintptr_t *ptrRet = (uintptr_t *)&returnAddress;
  for (int i = 0; i < 8; i++)
    shellcode.push_back(((uint8_t *)ptrRet)[i]);

  // 4. Write Shellcode to Trampoline
  if (!mem->WriteBuffer(trampolineAddress, shellcode.data(),
                        shellcode.size())) {
    std::cerr << "[Unlocker] Failed to write shellcode!" << std::endl;
    return false;
  }

  // 5. Install Hook (Overwrite Target)
  // Construct the 14-byte JMP to trampoline
  std::vector<uint8_t> hookCode;
  hookCode.push_back(0xFF);
  hookCode.push_back(0x25);
  hookCode.push_back(0x00);
  hookCode.push_back(0x00);
  hookCode.push_back(0x00);
  hookCode.push_back(0x00);
  uintptr_t *ptrTramp = (uintptr_t *)&trampolineAddress;
  for (int i = 0; i < 8; i++)
    hookCode.push_back(((uint8_t *)ptrTramp)[i]);

  // Pad with NOPs if stolen size > 14 (16 - 14 = 2 NOPs)
  for (size_t i = 0; i < (STOLEN_SIZE - 14); i++)
    hookCode.push_back(0x90);

  // Write the hook!
  // Since we are external and mapped pages might be protected, we should use
  // VirtualProtectEx (via Memory write if it handles it, or explicit)
  // Memory.write usually handles standard write. If it fails, we need
  // VirtualProtectEx. Assuming Ryujinx JIT pages are RWX (Execution requires X,
  // JIT writing requires W).
  if (!mem->WriteBuffer(targetAddress, hookCode.data(), hookCode.size())) {
    std::cerr << "[Unlocker] Failed to write JMP hook!" << std::endl;
    return false;
  }

  // Save Patch info
  Patch p;
  p.address = targetAddress;
  p.originalBytes = originalBytes;
  patches.push_back(p);

  return true;
}

bool Unlocker::NopInstruction(uintptr_t address) {
  if (address == 0)
    return false;

  // Standard NOP size for 'mov [rsi+rbp], eax' is 3 bytes (89 04 2E)
  const size_t SIZE = 3;
  std::vector<uint8_t> original(SIZE);

  if (!mem->ReadBuffer(address, original.data(), SIZE))
    return false;

  uint8_t nops[] = {0x90, 0x90, 0x90};
  if (!mem->WriteBuffer(address, nops, SIZE))
    return false;

  Patch p;
  p.address = address;
  p.originalBytes = original;
  patches.push_back(p);

  return true;
}

uintptr_t Unlocker::GetCameraBaseAddr() { return pCameraBase; }
uintptr_t Unlocker::GetCameraOffsetAddr() { return pCameraOffset; }

void Unlocker::Restore() {
  if (mem->IsOpen() && !patches.empty()) {
    // Restore in reverse order usually is safer, though here order shouldn't
    // matter much
    for (int i = patches.size() - 1; i >= 0; i--) {
      mem->WriteBuffer(patches[i].address, patches[i].originalBytes.data(),
                       patches[i].originalBytes.size());
    }
    patches.clear();
  }
}
