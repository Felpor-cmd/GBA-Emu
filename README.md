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
smoke test, pass `--frames 2`. A deterministic run and frame capture needs no display:

    build/gba_emu roms/Zelda.gba --headless --frames 600 --dump-frame /tmp/zelda.ppm

`--trace` prints the final register state and recent instructions. Unsupported
BIOS services produce one diagnostic and the trace, rather than entering empty
BIOS memory. Headless runs require a frame limit.

The SDL loop runs at the GBA frame period. The PPU renders bitmap modes 3–5,
tiled backgrounds in modes 0–2, and regular and affine sprites with layer
priority. VCOUNT, HBlank, and VBlank status advance with CPU cycles. Rendering
currently samples memory once per frame, so writes during a scanline do not
produce raster effects. BIOS-free boot supports the reset, copying, division,
affine setup, and interrupt wait services needed by Zelda: The Minish Cap.
Video IRQs, immediate/VBlank/HBlank DMA, alpha blending, brightness, and WIN0/1
clipping are implemented. Timers, audio, and keypad controls remain pending.

Local ROM screenshot comparisons against pinned mGBA are described in [docs/zelda-boot.md](docs/zelda-boot.md).

## Status

- [x] Project skeleton, ROM loading
- [x] CPU core (ARM + Thumb instruction sets)
- [x] Memory bus (address region routing)
- [x] PPU (base bitmap, tile, affine, and OBJ rendering)
- [x] Video interrupts and immediate/VBlank/HBlank DMA
- [ ] Timers and sound FIFO DMA
- [ ] Keypad input
- [ ] Sound

## References

- GBATEK (hardware reference): https://problemkaputt.de/gbatek.htm
- Test ROMs: https://github.com/jsmolka/gba-tests
- Reference implementation: https://github.com/nba-emu/NanoBoyAdvance
