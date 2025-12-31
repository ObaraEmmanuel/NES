#include <stdlib.h>

#include "mapper.h"
#include "emulator.h"


static uint8_t read_PRG(Mapper *mapper, uint16_t address);
static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value);
static uint8_t read_CHR(Mapper *mapper, uint16_t address);
static void write_CHR(Mapper *mapper, uint16_t address, uint8_t value);
static uint8_t read_ROM(Mapper *mapper, uint16_t address);
static void write_ROM(Mapper *mapper, uint16_t address, uint8_t value);
static void on_scanline(Mapper *mapper);
static void reset(Mapper *mapper);

typedef struct {
    // extra fields
    uint8_t reg1;
    // etc.
} extension_t;

int load_mapper_x(Mapper *mapper) {
    // load implementations
    // mapper will already be loaded with generic implementations
    // so, you should delete what you are not implementing
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;

    // These are not commonly overridden, proceed with caution.
    // mapper->read_ROM = read_ROM;
    // mapper->write_ROM = write_ROM;
    // mapper->on_scanline = on_scanline;
    // mapper->reset = reset;

    // allocate extension if necessary
    // the mapper struct should be adequate for most use cases
    // use mapper->PRG/CHR_ptrs to store pointers
    // use mapper->PRG/CHR_regs to store registers
    extension_t* ext = calloc(1, sizeof(extension_t));
    if (ext == NULL) {
        return -1;
    }
    mapper->extension = ext;
    return 0;
}

static void on_scanline(Mapper *mapper) {
    // called on each scanline
}

static uint8_t read_ROM(Mapper *mapper, uint16_t address) {
    // handles PRG 0x6000 - 0xFFFF
    // to return open bus, use mapper->emulator->mem.bus
    return mapper->emulator->mem.bus;
}

static void write_ROM(Mapper *mapper, uint16_t address, uint8_t value) {
    // handles PRG 0x6000 - 0xFFFF
}

static uint8_t read_PRG(Mapper *mapper, uint16_t address) {
    // handles PRG 0x8000 - 0xFFFF
    // to return open bus, use mapper->emulator->mem.bus
    return mapper->emulator->mem.bus;
}

static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value) {
    // handles PRG 0x8000 - 0xFFFF
}

static uint8_t read_CHR(Mapper *mapper, uint16_t address) {
    // handles PPU 0x0000 - 0x1FFF
    return 0;
}

static void write_CHR(Mapper *mapper, uint16_t address, uint8_t value) {
    // handles PPU 0x0000 - 0x1FFF
}
