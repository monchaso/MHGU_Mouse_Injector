# MHGU Mouse Injector: Final Standalone Migration Guide

This guide outlines the professional transition from a Cheat Engine environment to a standalone C++ application. It specifically covers high-performance scanning, instruction overriding (NOPing), and the integration of "isBusy" menu detection for a seamless experience.

---

## 1. Core Architecture

A professional injector operates through a strictly ordered sequence:

1. **Process Linkage**: Targeting `Ryujinx.exe` via `OpenProcess`.
2. **Signature Analysis**: Utilizing SIMD-accelerated scanning to find opcodes in memory.
3. **Overriding & Hooking**: Silencing the native camera logic (NOP) and redirecting to a custom trampoline.
4. **Input Processing**: A high-frequency background loop translates raw mouse movement into 3D coordinates.

---

## 2. High-Speed Memory Scanning (AOB)

Emulators like Ryujinx manage massive heaps (6GB+). To avoid prohibitively slow scans, you must optimize the search:

* **Region Filtering**: Use `VirtualQueryEx`. Only scan regions with `State == MEM_COMMIT` and `Protect == PAGE_EXECUTE_READ`. This skips data-heavy heaps and focuses on JIT-compiled code.
* **Block Reading**: Read memory in large chunks (1MB+) into a local buffer before scanning.
* **SIMD Comparison**: Use AVX2 intrinsics (`_mm256_cmpeq_epi8`) to compare 32 bytes at a time. This is orders of magnitude faster than standard C++ loops.

---

## 3. Instruction Overriding (NOPing)

To achieve true 1:1 mouse movement, you must stop the game's internal code from "fighting" your writes.

* **The Goal**: Locate the instruction `89 04 2E` (`mov [rsi+rbp], eax`).
* **The Override**: Change the memory protection to `PAGE_EXECUTE_READWRITE` and overwrite those bytes with `0x90` (NOP).
* **The Result**: The game logic for the controller still calculates, but it never "saves" the result to the camera address, leaving the memory open for your injector to write to.

---

## 4. The "isBusy" Flag & Gating

While a hotkey (F3) is used for toggling, the `isBusy` flag provides automatic logic gating.

* **Purpose**: Detects if a menu, item pouch, or map is open.
* **Mechanism**: A 1-byte memory address that reflects the game's UI state (e.g., `0` for gameplay, `1` for menu).
* **Implementation**:

```cpp
// Logic Guard
byte menuOpen = 0;
ReadProcessMemory(hProcess, (LPCVOID)isBusyAddress, &menuOpen, 1, NULL);
if (menuOpen == 1) {
    ShowCursor(true); // Release mouse to the OS
    return;           // Skip camera updates
}

```

---

## 5. Capturing Registers (RSI & RBP)

Since Ryujinx uses dynamic memory, you cannot hardcode addresses. You must capture the base pointers used by the game.

* **Capture Hook**: Your assembly trampoline captures `RSI` and `RBP` and saves them to a designated memory slot (`pCameraBase` and `pCameraOffset`).
* **Address Resolution**: Your C++ loop calculates the target address at runtime:
`FinalAddress = Read(pCameraBase) + Read(pCameraOffset)`.

---

## 6. Mouse Logic & Math

The external loop (running every 5ms-10ms) performs the following:

1. **Input Delta**: Calculates `CurrentMouse - ScreenCenter`.
2. **Horizontal (Yaw) Wrapping**: MHGU uses radians ( to ). If your value exceeds , subtract . If it goes below , add .
3. **Vertical (Pitch) Clamping**: **Strictly** clamp the Y-axis between  and . Never allow wrapping here, as it will flip the camera view.
4. **Memory Write**: Use `WriteProcessMemory` to send the new float to the resolved `FinalAddress`.

---

## 7. Recommended Libraries

* **Windows.h**: Standard for process and memory management.
* **MinHook**: Safely handles 64-bit absolute jumps and trampoline creation.
* **PatternScan.h**: A lightweight SIMD-ready scanner for AOBs.

---

## 8. Project File Structure

To maintain a clean codebase, keep the root directory clean and move logic to a `Core` subfolder:

*   **`main.cpp`**: (Root) Entry point. Handles the main loop, hotkey detection (F3), and high-level logic flow.
*   **`Core/Memory.h/cpp`**: Wrapper class for Windows API. Handles `OpenProcess`, `ReadProcessMemory`, `WriteProcessMemory`, and `VirtualQueryEx`.
*   **`Core/Scanner.h/cpp`**: Dedicated SIMD AOB scanner. Contains the logic to buffer memory regions and execute AVX2 searches.
*   **`Core/Unlocker.h/cpp`**: Handles the "Dirty Work". Manages the trampoline allocation, writing the JMP instruction, and NOPing the original code. Use `MinHook` logic here.
*   **`Core/MouseInput.h/cpp`**: Math utility. Handles the Raw Input reading, `Delta -> Yaw/Pitch` conversion, and clamping logic.

---

## 9. Summary Checklist

* [ ] **Process Attachment**: Verify handle with `PROCESS_ALL_ACCESS`.
* [ ] **SIMD Scan**: Locate `89 04 2E` in executable memory.
* [ ] **Hook & Override**: Apply 14-byte absolute jump and NOP the original write.
* [ ] **Capture**: Store `RSI` and `RBP` to stable memory locations.
* [ ] **Logic Loop**: Apply  wrapping for Yaw and clamping for Pitch.
* [ ] **Gating**: (Optional for now) Implement `isBusy` check to disable the mouse in menus.