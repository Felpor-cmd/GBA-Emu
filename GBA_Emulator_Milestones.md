# GBA Emulator — Milestone Roadmap

A full, granular checklist from where you are now to a playable emulator.
Milestones are kept small on purpose — each one should be a single sitting's
worth of work, something you can finish, test, and feel done with before
moving to the next box.

---

## Phase 0 — Project Foundations ✅ DONE

- [x] C++ project skeleton with CMake
- [x] Load a ROM file into memory
- [x] CPU register file skeleton (`regs_[16]`, `cpsr_`)
- [x] `Reset()` sets post-BIOS register state (skip real BIOS boot)

## Phase 1 — Memory Bus ✅ DONE

- [x] EWRAM read/write
- [x] IWRAM read/write
- [x] Palette RAM read/write
- [x] VRAM read/write
- [x] OAM read/write
- [x] BIOS read (zeroed placeholder, writes ignored)
- [x] I/O register placeholder read/write (dumb array, no side effects yet)
- [x] ROM read (all three mirrored address windows, bounds-checked against loaded file size)
- [x] SRAM read/write

---

## Phase 2 — CPU Core (ARM7TDMI)

### 2a. Fetch/decode skeleton
- [x] `Step()` fetches a raw instruction word from the bus at the PC and advances the PC (does nothing else yet)
- [x] Branch fetch width on the CPSR `T` bit (32-bit ARM vs 16-bit Thumb)
- [x] Condition code evaluation (top 4 bits of an ARM instruction vs CPSR flags)

### 2b. ARM instruction set
- [x] Data processing — register operand (MOV, ADD, SUB, CMP, AND, ORR, ...)
- [x] Data processing — immediate operand
- [x] Data processing — shifted register operand (LSL, LSR, ASR, ROR)
- [x] Branch (B) and Branch-with-Link (BL)
- [x] Branch and Exchange (BX) — this is what actually switches into Thumb state
- [x] Single data transfer (LDR/STR, word and byte)
- [x] Halfword and signed transfers (LDRH/STRH/LDRSB/LDRSH)
- [x] Block data transfer (LDM/STM) — push/pop and context save rely on this
- [ ] Multiply and multiply-accumulate (MUL, MLA)
- [ ] PSR transfer (MRS/MSR) — direct read/write of CPSR and SPSR
- [ ] Software interrupt (SWI) — a stub that just logs "BIOS call requested" is a fine first version

### 2c. Thumb instruction set
- [ ] Move shifted register
- [ ] Add/subtract
- [ ] Move/compare/add/subtract immediate
- [ ] ALU operations
- [ ] Hi register operations / branch exchange
- [ ] PC-relative load
- [ ] Load/store with register offset
- [ ] Load/store sign-extended byte/halfword
- [ ] Load/store with immediate offset
- [ ] Load/store halfword
- [ ] SP-relative load/store
- [ ] Load address
- [ ] Add offset to stack pointer
- [ ] Push/pop registers
- [ ] Multiple load/store
- [ ] Conditional branch
- [ ] Software interrupt
- [ ] Unconditional branch
- [ ] Long branch with link

### 2d. Validate against test ROMs
- [ ] Get the ARM opcode test suite running at all (even if it reports failures)
- [ ] Get the full ARM suite passing
- [ ] Get the Thumb opcode test suite running
- [ ] Get the full Thumb suite passing

---

## Phase 3 — A Window You Can See

You'll want this before or alongside the PPU — testing pixels is much easier
when you can actually look at them.

- [ ] Add a windowing/graphics library to the build (SDL2 is the most common choice for this)
- [ ] Open a blank window
- [ ] Draw one solid test color to the window, proving the pixel pipeline works end to end
- [ ] Add a fixed-timestep main loop (so the emulator doesn't run at uncapped, unpredictable speed)

---

## Phase 4 — PPU (Video)

- [ ] Bitmap mode 3 (simplest possible: one full-screen 16-bit-color framebuffer)
- [ ] Bitmap mode 4 (paletted framebuffer with page flipping)
- [ ] Bitmap mode 5 (small paletted framebuffer)
- [ ] Tile-based background mode 0 (regular tiled backgrounds, up to 4 layers)
- [ ] Tile-based background mode 1
- [ ] Tile-based background mode 2 (affine/rotated backgrounds)
- [ ] Sprite (OBJ) rendering — regular sprites
- [ ] Sprite (OBJ) rendering — affine (rotated/scaled) sprites
- [ ] Priority and layering between backgrounds and sprites
- [ ] *(optional polish)* Mosaic effect
- [ ] *(optional polish)* Alpha blending
- [ ] *(optional polish)* Windowing (the GBA's clipping-region feature, not an OS window)

---

## Phase 5 — Interrupts

- [ ] Give `IE`, `IF`, and `IME` (the interrupt control registers) real behavior instead of just sitting in the dumb I/O array
- [ ] CPU exception entry for IRQ: mode switch, saving `LR`/`SPSR`, jumping to the interrupt vector
- [ ] Return-from-interrupt handling
- [ ] VBlank interrupt actually firing at the right time
- [ ] HBlank interrupt actually firing at the right time

---

## Phase 6 — Timers

- [ ] One free-running, up-counting timer register
- [ ] Timer overflow triggering an interrupt
- [ ] Timer cascading (one timer's overflow feeding the next)

---

## Phase 7 — DMA

- [ ] Immediate (one-shot) DMA transfer
- [ ] VBlank-triggered DMA
- [ ] HBlank-triggered DMA
- [ ] Sound FIFO DMA (you'll come back to wire this up properly once sound exists)

---

## Phase 8 — Input

- [ ] Keypad register reflects real keyboard or controller state
- [ ] *(optional)* Keypad interrupt (rarely used by real games, low priority)

---

## Phase 9 — Sound

- [ ] The 4 legacy Game-Boy-style channels
- [ ] The 2 direct sound (DMA-fed) channels
- [ ] Mixing all channels together and outputting real audio

---

## Phase 10 — Save Types Beyond Plain SRAM

Different cartridges used different save chips — SRAM was the simple case
you already built.

- [ ] Flash memory save support
- [ ] EEPROM save support
- [ ] Persist save data to a file on disk between runs (right now everything resets when the program closes)

---

## Phase 11 — Timing Accuracy & Compatibility

This is the long tail — the phase where "it boots" turns into "it's actually
correct."

- [ ] Instruction timing that accounts for cycles, not just correctness
- [ ] Pass full test ROM suites: ARM, Thumb, timing, and PPU edge cases
- [ ] Boot and play a handful of real commercial games
- [ ] Track and fix game-specific quirks as they show up

---

## Phase 12 — Nice-to-Haves (entirely optional, in any order)

- [ ] Save states
- [ ] Fast-forward / rewind
- [ ] A debugger: breakpoints, memory viewer, disassembler
- [ ] Real BIOS emulation (replacing the current boot-skip shortcut)
- [ ] Link cable / multiplayer emulation (even mature emulators often skip this)

---

## Notes

2b.1 One honest thing to flag and set aside for now: on real hardware, if Rn or Rm is r15 (the PC), the value read isn't quite the PC you'd expect — a pipelining quirk makes it read as current instruction address + 8. We're not modeling a pipeline, so this'll be slightly wrong if a game ever uses the PC as a math operand. It's a real gap, but a narrow one — safe to note and revisit later rather than solve today.
