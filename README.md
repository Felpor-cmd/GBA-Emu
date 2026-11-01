# gba-emu

A Game Boy Advance emulator built from scratch, one subsystem at a time.

## Build

    cmake -S . -B build
    cmake --build build

SDL2 development headers are required.

## Run

    build/gba_emu path/to/rom.gba

Press Escape to close the window. To see the pixel pipeline without a ROM:

    build/gba_emu --test-pattern

The test pattern is a solid red Mode 3 framebuffer. For an automated display
smoke test, pass `--frames 2`; SDL's dummy video driver can run it headlessly.

The SDL loop runs at the GBA frame period. The PPU renders bitmap modes 3–5,
tiled backgrounds in modes 0–2, and regular and affine sprites with layer
priority. VCOUNT, HBlank, and VBlank status advance with CPU cycles. Rendering
currently samples memory once per frame, so writes during a scanline do not
produce raster effects. Interrupt delivery, DMA, and audio are still to come.

## Status

- [x] Project skeleton, ROM loading
- [x] CPU core (ARM + Thumb instruction sets)
- [x] Memory bus (address region routing)
- [x] PPU (base bitmap, tile, affine, and OBJ rendering)
- [ ] Timers, DMA, interrupts
- [ ] Keypad input
- [ ] Sound

## References

- GBATEK (hardware reference): https://problemkaputt.de/gbatek.htm
- Test ROMs: https://github.com/jsmolka/gba-tests
- Reference implementation: https://github.com/nba-emu/NanoBoyAdvance
