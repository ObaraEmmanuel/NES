#pragma once

#include <stdint.h>

#include "ppu.h"
#include "mapper.h"
#include "controller.h"

#define IRQ_ADDRESS 0xFFFE
#define NMI_ADDRESS 0xFFFA
#define RESET_ADDRESS 0xFFFC
#define RAM_SIZE 0x800
#define RAM_END 0x2000
#define IO_REG_MIRRORED_END 0x4000
#define IO_REG_END 0x4020

typedef enum{
    PPU_CTRL = 0x2000,
    PPU_MASK,
    PPU_STATUS,
    OAM_ADDR,
    OAM_DATA,
    PPU_SCROLL,
    PPU_ADDR,
    PPU_DATA,

    APU_P1_CTRL = 0x4000,
    APU_P1_RAMP,
    APU_P1_FT,
    APU_P1_CT,

    APU_P2_CTRL,
    APU_P2_RAMP,
    APU_P2_FT,
    APU_P2_CT,

    APU_TRI_LINEAR_COUNTER,
    APU_TRI_FREQ1 = 0x400A,
    APU_TRI_FREQ2,

    APU_NOISE_CTRL,
    APU_NOISE_FREQ1 = 0x400E,
    APU_NOISE_FREQ2,

    APU_DMC_CTRL,
    APU_DMC_DA,
    APU_DMC_ADDR,
    APU_DMC_LEN,

    OAM_DMA,

    APU_CTRL,
    APU_STATUS = 0x4015,
    JOY1,
    JOY2,
    FRAME_COUNTER = 0x4017


} IORegister;

struct Emulator;

typedef struct Memory {
    uint8_t RAM[RAM_SIZE];
    uint8_t bus;
    JoyPad joy1;
    JoyPad joy2;
    Mapper* mapper;
    struct Emulator* emulator;
} Memory;

void init_mem(struct Emulator* emulator);
void write_mem(Memory* mem, uint16_t address, uint8_t value);
uint8_t read_mem(Memory* mem, uint16_t address);
uint8_t* get_ptr(Memory* mem, uint16_t address);