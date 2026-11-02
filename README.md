# gba-emu

A Game Boy Advance emulator built from scratch, one subsystem at a time.

## Build

    cmake -S . -B build
    cmake --build build

SDL2 2.0.18 or newer development headers are required. CMake fetches the pinned
Dear ImGui docking source used by the input settings window.

## Run

    build/gba_emu path/to/rom.gba

Press F1 to open the emulator menu, then choose **Input Configuration** to
select a controller and remap keyboard or controller bindings. Keyboard and the
selected controller can be used together. Defaults are WASD for movement, J/K
for A/B, U/I for L/R, Enter for Start, and Right Shift for Select. Press Escape
to close the game window. Hold Tab to fast-forward; select 1x, 2x, 3x, 5x, or
10x in the F1 input menu. Actual speed depends on the game and computer. To see
the pixel pipeline without a ROM:

    build/gba_emu --test-pattern

The test pattern is a solid red Mode 3 framebuffer. For an automated display
smoke test, pass `--frames 2`. A deterministic run and frame capture needs no display:

    mkdir -p /tmp/gba-emu/Zelda/captures
    build/gba_emu roms/Zelda.gba --headless --no-save --frames 600 \
        --dump-frame /tmp/gba-emu/Zelda/captures/zelda-600.ppm

Full power-on boot, including the BIOS startup screen:

    build/gba_emu roms/Zelda.gba --bios /home/arch/Downloads/gba_bios.bin

`--trace` prints the final register state and recent instructions. Unsupported
BIOS services produce a diagnostic and final register state. Headless runs
require a frame limit.

Cartridge saves use a raw `.sav` file beside the ROM. SRAM, 64/128 KiB Flash,
and 512 B/8 KiB EEPROM are supported. The emulator loads the file on startup
and saves changes every five seconds and when it exits. Use `--save-path FILE`
to choose another location, `--save-type sram|flash64|flash128|eeprom512|eeprom8k`
for a ROM whose save type cannot be detected, or `--no-save` for an isolated run.

The SDL loop runs at the GBA frame period. The PPU renders bitmap modes 3–5,
tiled backgrounds in modes 0–2, and regular and affine sprites with layer
priority. VCOUNT, HBlank, and VBlank status advance with CPU cycles. Rendering
currently samples memory once per frame, so writes during a scanline do not
produce raster effects. BIOS-free boot supports the reset, copying, division,
affine setup, LZ77 work RAM/VRAM decompression, and interrupt wait services
needed by Zelda: The Minish Cap. `--bios` requires a readable 16 KiB GBA BIOS;
the file is loaded in place and is never copied into the repository.
Timers, video IRQs, DMA, alpha blending, brightness, WIN0/1 clipping,
keyboard/controller keypad input, and audio are implemented. Keypad interrupts
remain pending.

Local ROM checks against pinned mGBA keep per-game logs and temporary captures in `/tmp/gba-emu/<ROM name>/`. Successful checks remove their captures and retain logs. See [docs/zelda-boot.md](docs/zelda-boot.md).

The deterministic file-selection input replay is
`test/fixtures/file_select_start_a.input`; it contains zero-based frame ranges
and GBA key masks, not game data. Use `--headless --frames 2500
--replay-input test/fixtures/file_select_start_a.input --expect-swi-12` to
verify that a compatible game reaches LZ77 decompression. To compare any
compatible ROMs with pinned mGBA in both boot modes, configure
`MGBA_ROOT`, `GBA_BIOS_PATH`, and the semicolon-separated
`GBA_BOOT_REPLAY_ROMS` CMake variable. CMake makes one BIOS and one BIOS-free
replay check per ROM, using its adjacent `.sav` file when present.
`GBA_BOOT_REPLAY_FRAMES` defaults to `2500,6000`, checking both file selection
and later gameplay. Set it to other checkpoints for a compatible game with
different scene timing. Verification copies saves to scratch files.

## Status

- [x] Project skeleton, ROM loading
- [x] CPU core (ARM + Thumb instruction sets)
- [x] Memory bus (address region routing)
- [x] PPU (base bitmap, tile, affine, and OBJ rendering)
- [x] Video interrupts and immediate/VBlank/HBlank DMA
- [x] Timers (prescalers, overflow IRQs, and cascading)
- [x] Keypad input (keyboard and controller remapping)
- [x] Sound FIFO DMA
- [ ] Keypad interrupt
- [x] Sound
- [x] SRAM, Flash, and EEPROM saves with disk persistence

## References

- GBATEK (hardware reference): https://problemkaputt.de/gbatek.htm
- Test ROMs: https://github.com/jsmolka/gba-tests
- Reference implementation: https://github.com/nba-emu/NanoBoyAdvance
