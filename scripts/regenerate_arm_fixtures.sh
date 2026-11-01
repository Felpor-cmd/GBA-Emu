#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/.." && pwd)"
fixture_dir="$root_dir/test/fixtures/arm"
temp_dir="$(mktemp -d)"
trap 'rm -rf "$temp_dir"' EXIT

arm-none-eabi-as -mcpu=arm7tdmi -o "$temp_dir/sequence.o" "$fixture_dir/sequence.s"
arm-none-eabi-ld -Ttext=0x08000000 -o "$temp_dir/sequence.elf" "$temp_dir/sequence.o"
arm-none-eabi-objcopy -O binary "$temp_dir/sequence.elf" "$fixture_dir/sequence.gba"
truncate -s 192 "$fixture_dir/sequence.gba"

arm-none-eabi-as -mcpu=arm7tdmi -o "$temp_dir/multiply_block.o" "$fixture_dir/multiply_block.s"
arm-none-eabi-ld -Ttext=0x08000000 -o "$temp_dir/multiply_block.elf" "$temp_dir/multiply_block.o"
arm-none-eabi-objcopy -O binary "$temp_dir/multiply_block.elf" "$fixture_dir/multiply_block.gba"
truncate -s 192 "$fixture_dir/multiply_block.gba"

for name in thumb_arithmetic thumb_memory; do
    arm-none-eabi-as -mcpu=arm7tdmi -o "$temp_dir/$name.o" "$fixture_dir/$name.s"
    arm-none-eabi-ld -Ttext=0x08000000 -o "$temp_dir/$name.elf" "$temp_dir/$name.o"
    arm-none-eabi-objcopy -O binary "$temp_dir/$name.elf" "$fixture_dir/$name.gba"
    truncate -s 192 "$fixture_dir/$name.gba"
done

arm-none-eabi-as -mcpu=arm7tdmi -o "$temp_dir/bios.o" "$fixture_dir/exception_bios.s"
arm-none-eabi-ld -Ttext=0 -o "$temp_dir/bios.elf" "$temp_dir/bios.o"
arm-none-eabi-objcopy -O binary "$temp_dir/bios.elf" "$fixture_dir/exception_bios.bin"
truncate -s 16384 "$fixture_dir/exception_bios.bin"
