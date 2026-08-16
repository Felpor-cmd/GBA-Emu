# gba-emu (starter scaffold)

A Game Boy Advance emulator built from scratch, one subsystem at a time.

## Build

    mkdir build && cd build
    cmake ..
    cmake --build .

## Run

    ./gba_emu path/to/rom.gba

Right now this just loads the ROM into memory and resets the CPU register
file -- `Cpu::Step()` and the real `Bus` read/write routing are both stubs.
That's intentional: get this compiling and running first, then fill in one
piece at a time.

## Status

- [x] Project skeleton, ROM loading
- [ ] CPU core (ARM + Thumb instruction sets)
- [ ] Memory bus (real address region routing)
- [ ] PPU (video)
- [ ] Timers, DMA, interrupts
- [ ] Keypad input
- [ ] Sound

## References

- GBATEK (hardware reference): https://problemkaputt.de/gbatek.htm
- Test ROMs: https://github.com/jsmolka/gba-tests
- Reference implementation: https://github.com/nba-emu/NanoBoyAdvance
