#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "memory/bus.hpp"

TEST_CASE("Bus Write8/Read8 Round Trip") {
    // Empty ROM is fine for this test since we are not testing ROM space.
    std::vector<u8> empty_rom{};
    Bus bus(empty_rom);
    
    // Pick a test value.
    const u8 test_value = 42;
    
    // Memory region base addresses
    const u32 ewram_base = 0x02000000;
    const u32 iwram_base = 0x03000000;
    const u32 palette_base = 0x05000000;
    const u32 vram_base = 0x06000000;
    const u32 oam_base = 0x07000000;
    
    SECTION("EWRAM") {
        // Write to a specific address within EWRAM. We add a small offset to ensure it works.
        const u32 address = ewram_base + 0x1234;
        bus.Write8(address, test_value);
        REQUIRE(bus.Read8(address) == test_value);
    }
    
    SECTION("IWRAM") {
        const u32 address = iwram_base + 0x456;
        bus.Write8(address, test_value);
        REQUIRE(bus.Read8(address) == test_value);
    }
    
    SECTION("Palette") {
        const u32 address = palette_base + 0x10;
        bus.Write8(address, test_value);
        REQUIRE(bus.Read8(address) == test_value);
    }
    
    SECTION("VRAM") {
        const u32 address = vram_base + 0x5678;
        bus.Write8(address, test_value);
        REQUIRE(bus.Read8(address) == test_value);
    }
    
    SECTION("OAM") {
        const u32 address = oam_base + 0x20;
        bus.Write8(address, test_value);
        REQUIRE(bus.Read8(address) == test_value);
    }
}

TEST_CASE("Bus Write8/Read8 Read-Only Regions") {
    std::vector<u8> dummy_rom{0x99, 0x88, 0x77, 0x66};
    Bus bus(dummy_rom);
    
    SECTION("BIOS") {
        const u32 address = 0x00000123;
        const u8 original_val = bus.Read8(address); // Expect 0 since BIOS is zero-initialized
        
        bus.Write8(address, 42);
        
        REQUIRE(bus.Read8(address) == original_val);

        // BIOS starts empty (all zeros) since we haven't loaded real firmware
        REQUIRE(bus.Read8(0x00000000) == 0);

        // BIOS write should not stick
        bus.Write8(0x00000010, 99);
        REQUIRE(bus.Read8(0x00000010) == 0);
    }
    
    SECTION("ROM") {
        const u32 address = 0x08000000;
        const u8 original_val = bus.Read8(address);
        
        // Attempt a write
        bus.Write8(address, 42);
        
        // Value must remain unchanged
        REQUIRE(bus.Read8(address) == original_val);
    }
}

TEST_CASE("Bus Write8/Read8 Edge-of-the-floor") {
    std::vector<u8> empty_rom{};
    Bus bus(empty_rom);
    
    const u8 first_byte_val = 0xAA;
    const u8 last_byte_val = 0xBB;
    
    SECTION("EWRAM") {
        const u32 base = 0x02000000;
        const u32 last = base + kEwramSize - 1;
        
        bus.Write8(base, first_byte_val);
        bus.Write8(last, last_byte_val);
        
        REQUIRE(bus.Read8(base) == first_byte_val);
        REQUIRE(bus.Read8(last) == last_byte_val);
    }
    
    SECTION("IWRAM") {
        const u32 base = 0x03000000;
        const u32 last = base + kIwramSize - 1;
        
        bus.Write8(base, first_byte_val);
        bus.Write8(last, last_byte_val);
        
        REQUIRE(bus.Read8(base) == first_byte_val);
        REQUIRE(bus.Read8(last) == last_byte_val);
    }
    
    SECTION("Palette") {
        const u32 base = 0x05000000;
        const u32 last = base + kPaletteSize - 1;
        
        bus.Write8(base, first_byte_val);
        bus.Write8(last, last_byte_val);
        
        REQUIRE(bus.Read8(base) == first_byte_val);
        REQUIRE(bus.Read8(last) == last_byte_val);
    }
    
    SECTION("VRAM") {
        const u32 base = 0x06000000;
        const u32 last = base + kVramSize - 1;
        
        bus.Write8(base, first_byte_val);
        bus.Write8(last, last_byte_val);
        
        REQUIRE(bus.Read8(base) == first_byte_val);
        REQUIRE(bus.Read8(last) == last_byte_val);
    }
    
    SECTION("OAM") {
        const u32 base = 0x07000000;
        const u32 last = base + kOamSize - 1;
        
        bus.Write8(base, first_byte_val);
        bus.Write8(last, last_byte_val);
        
        REQUIRE(bus.Read8(base) == first_byte_val);
        REQUIRE(bus.Read8(last) == last_byte_val);
    }
}

TEST_CASE("Bus Write8/Read8 Don't-crash (Mirroring) test") {
    std::vector<u8> empty_rom{};
    Bus bus(empty_rom);
    
    const u8 test_val = 0x55;
    
    SECTION("EWRAM") {
        const u32 base = 0x02000000;
        const u32 out_of_bounds = base + kEwramSize + 0x10;
        const u32 wrapped = base + ((out_of_bounds - base) & (kEwramSize - 1));
        
        bus.Write8(out_of_bounds, test_val);
        
        REQUIRE(bus.Read8(out_of_bounds) == test_val);
        REQUIRE(bus.Read8(wrapped) == test_val);
    }
    
    SECTION("IWRAM") {
        const u32 base = 0x03000000;
        const u32 out_of_bounds = base + kIwramSize + 0x20;
        const u32 wrapped = base + ((out_of_bounds - base) & (kIwramSize - 1));
        
        bus.Write8(out_of_bounds, test_val);
        
        REQUIRE(bus.Read8(out_of_bounds) == test_val);
        REQUIRE(bus.Read8(wrapped) == test_val);
    }
    
    SECTION("Palette") {
        const u32 base = 0x05000000;
        const u32 out_of_bounds = base + kPaletteSize + 0x30;
        const u32 wrapped = base + ((out_of_bounds - base) & (kPaletteSize - 1));
        
        bus.Write8(out_of_bounds, test_val);
        
        REQUIRE(bus.Read8(out_of_bounds) == test_val);
        REQUIRE(bus.Read8(wrapped) == test_val);
    }
    
    SECTION("VRAM") {
        const u32 base = 0x06000000;
        const u32 out_of_bounds = base + kVramSize + 0x40;
        // VRAM size (0x18000) is not a power of 2, so the mask 0x17FFF
        // maps 0x18040 to 0x10040, which is still inside the floor!
        const u32 wrapped = base + ((out_of_bounds - base) & (kVramSize - 1));
        
        bus.Write8(out_of_bounds, test_val);
        
        REQUIRE(bus.Read8(out_of_bounds) == test_val);
        REQUIRE(bus.Read8(wrapped) == test_val);
    }
    
    SECTION("OAM") {
        const u32 base = 0x07000000;
        const u32 out_of_bounds = base + kOamSize + 0x50;
        const u32 wrapped = base + ((out_of_bounds - base) & (kOamSize - 1));
        
        bus.Write8(out_of_bounds, test_val);
        
        REQUIRE(bus.Read8(out_of_bounds) == test_val);
        REQUIRE(bus.Read8(wrapped) == test_val);
    }
}

TEST_CASE("Bus Write32/Read8 Byte-order (Endianness) test") {
    std::vector<u8> empty_rom{};
    Bus bus(empty_rom);
    
    // We'll use EWRAM to test the byte order.
    const u32 address = 0x02000000;
    const u32 test_val32 = 0x12345678;
    
    bus.Write32(address, test_val32);
    
    // GBA is little-endian: least significant byte is at the lowest address.
    REQUIRE(bus.Read8(address + 0) == 0x78);
    REQUIRE(bus.Read8(address + 1) == 0x56);
    REQUIRE(bus.Read8(address + 2) == 0x34);
    REQUIRE(bus.Read8(address + 3) == 0x12);
}
