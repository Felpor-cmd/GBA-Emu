# jsmolka/gba-tests opcode ROMs

These unmodified ARM and Thumb opcode ROMs are from
[`jsmolka/gba-tests`](https://github.com/jsmolka/gba-tests), pinned at commit
`a7113b67e63f83a9b321696ddd7042ccfad6c881`. They are distributed under the
adjacent MIT license. `SHA256SUMS` records the fixture digests and is checked
in CI.

The upstream ROMs execute their original instruction tests until the
`m_vsync` evaluation loop. The ARM loop begins at `0x08001D4C` and stores its
result in `r12`; the Thumb loop begins at `0x08000934` and stores its result in
`r7`. `opcode_rom_runner` checks the loop signature and reports the first
nonzero upstream test ID. Run both tests with
`ctest --test-dir build -R '^opcode_(arm|thumb)$' --output-on-failure`.
