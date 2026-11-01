# ARM decode and timing validation

`Cpu::Step()` returns clocks spent on the instruction and `GetCycles()` returns
the total since `Reset()`. The CPU charges opcode fetches, data accesses,
internal cycles, and two refill accesses after a PC change. `Bus::AccessCycles`
applies GBA region width and WAITCNT values. The Game Pak prefetch model uses
an eight-halfword timing credit and fills it during modeled idle Game Pak bus
clocks. It does not yet include DMA or PPU contention, so full game timing
remains outside this validation scope.

## Coverage audit

| ARM family | Existing behavior tests | New timing / trace validation |
| --- | --- | --- |
| Data processing, shifts, PSR | `cpu_test_data_processing_*`, `cpu_test_psr_transfer` | sequence fixture, shifted-register and PC refill timing |
| Branch, BX, PC writes | `cpu_test_branch_*`, `cpu_test_pc_behavior` | branch/refill clocks, sequence trace |
| Single and halfword transfers, SWP | `cpu_test_single_data_transfer`, `cpu_test_halfword_and_signed_transfers`, `cpu_test_single_data_swap` | load/store/SWP clocks, sequence trace |
| Block transfers | `cpu_test_block_data_transfer`, `cpu_test_user_mode_transfer` | register-count clocks, multiply/block trace |
| Multiply and long multiply | `cpu_test_multiply` | Rs-dependent clocks, multiply/block trace |
| SWI, Undefined, coprocessor | `cpu_test_swi`, `cpu_test_coprocessor` | invalid ARMv5 words, synthetic BIOS return, exception refill |
| Conditions | `cpu_test_condition_code_evaluation` and per-family cases | failed condition on reserved opcode has no exception side effect |

The checked-in `test/fixtures/arm/*.gba` files are raw synthetic ROMs. The
16 KiB synthetic BIOS contains handlers at the Undefined and SWI vectors.
Regenerate them with `scripts/regenerate_arm_fixtures.sh` after installing
`binutils-arm-none-eabi`; CI checks that regeneration reproduces the binaries.

To run the trace comparator locally, build [mGBA 0.10.5](https://github.com/mgba-emu/mgba/releases/tag/0.10.5)
as a shared library, install it to a private prefix, then configure with
`-DMGBA_ROOT=/path/to/prefix` and run `ctest --test-dir build -R mgba_arm_`.
The comparator checks registers, CPSR, PC, and IWRAM after each instruction;
it prints the first differing field. It uses mGBA's [C core API](https://github.com/mgba-emu/mgba/blob/0.10.5/include/mgba/core/core.h)
because mGBA 0.10.5 does not offer a command-line script loader.

Clock expectations come from the [ARM7TDMI-S cycle tables](https://documentation-service.arm.com/static/5e8e1bf9fd977155116a4658)
and [GBATEK's WAITCNT/Game Pak timing rules](https://mgba-emu.github.io/gbatek/).
Reference traces validate architectural state; they are not treated as clock
oracles.
