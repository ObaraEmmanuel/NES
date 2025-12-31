#include "mapper.h"
#include "utils.h"

static uint8_t read_PRG(Mapper *, uint16_t);
static void write_PRG(Mapper *, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper *, uint16_t);
static void write_ROM(Mapper *mapper, uint16_t address, uint8_t value);
static void reset(Mapper *mapper);
static void select_banks(Mapper *mapper);

typedef struct {
    uint8_t CHR;
    uint8_t PRG;
} reg_t;

int load_colordreams(Mapper *mapper) {
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->PRG_ptrs[0] = mapper->PRG_ROM;
    mapper->CHR_ptrs[0] = mapper->CHR_ROM;
    return 0;
}

int load_colordreams46(Mapper *mapper) {
    // two store the two registers
    // CHR = 0, PRG = 1
    mapper->extension = calloc(1, sizeof(reg_t));
    mapper->write_ROM = write_ROM;
    mapper->read_PRG = read_PRG;
    mapper->read_CHR = read_CHR;
    mapper->PRG_ptrs[0] = mapper->PRG_ROM;
    mapper->CHR_ptrs[0] = mapper->CHR_ROM;
    mapper->reset = reset;
    return 0;
}

void reset(Mapper *mapper) {
    write_ROM(mapper, 0x6000, 0);
    write_ROM(mapper, 0x8000, 0);
}

static void write_ROM(Mapper *mapper, uint16_t address, uint8_t value) {
    if (address < 0x6000) {
        LOG(DEBUG, "Attempted to write to unavailable expansion ROM");
        return;
    }

    if (address < 0x8000) {
        reg_t *reg = mapper->extension;
        reg->CHR &= 0x7;
        reg->CHR |= (value & 0xf0) >> 1;
        reg->PRG &= 0x1;
        reg->PRG |= (value & 0xf) << 1;
        select_banks(mapper);
        return;
    }

    /*
    7  bit  0
    ---- ----
    .CCC ...P
     |||    |
     |||    +- PRG LOW
     +++------ CHR LOW
    */
    reg_t *reg = mapper->extension;
    reg->CHR &= ~0x7;
    reg->CHR |= (value & 0x70) >> 4;
    reg->PRG &= ~0x1;
    reg->PRG |= value & 0x1;
    select_banks(mapper);
}

static void select_banks(Mapper *mapper) {
    const reg_t *reg = mapper->extension;
    mapper->PRG_ptrs[0] = mapper->PRG_ROM + reg->PRG * 0x8000;
    mapper->CHR_ptrs[0] = mapper->CHR_ROM + reg->CHR * 0x2000;
}

static uint8_t read_PRG(Mapper *mapper, uint16_t address) {
    return *(mapper->PRG_ptrs[0] + (address - 0x8000));
}

static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value) {
    /*
    7  bit  0
    ---- ----
    CCCC LLPP
    |||| ||||
    |||| ||++- Select 32 KB PRG ROM bank for CPU $8000-$FFFF
    |||| ++--- Used for lockout defeat
    ++++------ Select 8 KB CHR ROM bank for PPU $0000-$1FFF
    */
    mapper->PRG_ptrs[0] = mapper->PRG_ROM + (value & 0x3) * 0x8000;
    mapper->CHR_ptrs[0] = mapper->CHR_ROM + 0x2000 * ((value >> 4) & 0xf);
}

static uint8_t read_CHR(Mapper *mapper, uint16_t address) {
    return *(mapper->CHR_ptrs[0] + address);
}

