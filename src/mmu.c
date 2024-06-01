#include <string.h>
#include "mmu.h"
#include "emulator.h"
#include "utils.h"

void init_mem(Emulator* emulator){
    Memory* mem = &emulator->mem;
    mem->emulator = emulator;
    mem->mapper = &emulator->mapper;

    memset(mem->RAM, 0, RAM_SIZE);
    init_joypad(&mem->joy1, 0);
    init_joypad(&mem->joy2, 1);
}

uint8_t* get_ptr(Memory* mem, uint16_t address){
    if(address < 0x2000)
        return mem->RAM + (address % 0x800);
    if(address > 0x6000 && address < 0x8000 && mem->mapper->save_RAM != NULL)
        return mem->mapper->save_RAM + (address - 0x6000);
    return NULL;
}

void write_mem(Memory* mem, uint16_t address, uint8_t value){

    if(address < RAM_END) {
        mem->RAM[address % RAM_SIZE] = value;
        return;
    }

    // resolve mirrored registers
    if(address < IO_REG_MIRRORED_END)
        address = 0x2000 + (address - 0x2000) % 0x8;

    // handle all IO registers
    if(address < IO_REG_END){
        PPU* ppu = &mem->emulator->ppu;
        APU* apu = &mem->emulator->apu;

        switch (address) {
            case PPU_CTRL:
                set_ctrl(ppu, value);
                break;
            case PPU_MASK:
                ppu->mask = value;
                break;
            case PPU_SCROLL:
                set_scroll(ppu, value);
                break;
            case PPU_ADDR:
                set_address(ppu, value);
                break;
            case PPU_DATA:
                write_ppu(ppu, value);
                break;
            case OAM_ADDR:
                set_oam_address(ppu, value);
                break;
            case OAM_DMA:
                dma(ppu, value);
                break;
            case OAM_DATA:
                write_oam(ppu, value);
                break;
            case JOY1:
                write_joypad(&mem->joy1, value);
                write_joypad(&mem->joy2, value);
                break;
            case APU_P1_CTRL:
                set_pulse_ctrl(&apu->pulse1, value);
                break;
            case APU_P2_CTRL:
                set_pulse_ctrl(&apu->pulse2, value);
                break;
            case APU_P1_RAMP:
                set_pulse_sweep(&apu->pulse1, value);
                break;
            case APU_P2_RAMP:
                set_pulse_sweep(&apu->pulse2, value);
                break;
            case APU_P1_FT:
                set_pulse_timer(&apu->pulse1, value);
                break;
            case APU_P2_FT:
                set_pulse_timer(&apu->pulse2, value);
                break;
            case APU_P1_CT:
                set_pulse_length_counter(&apu->pulse1, value);
                break;
            case APU_P2_CT:
                set_pulse_length_counter(&apu->pulse2, value);
                break;
            case APU_TRI_LINEAR_COUNTER:
                set_tri_counter(&apu->triangle, value);
                break;
            case APU_TRI_FREQ1:
                set_tri_timer_low(&apu->triangle, value);
                break;
            case APU_TRI_FREQ2:
                set_tri_length(&apu->triangle, value);
                break;
            case APU_NOISE_CTRL:
                set_noise_ctrl(&apu->noise, value);
                break;
            case APU_NOISE_FREQ1:
                set_noise_period(apu, value);
                break;
            case APU_NOISE_FREQ2:
                set_noise_length(&apu->noise, value);
                break;
            case APU_DMC_ADDR:
                set_dmc_addr(&apu->dmc, value);
                break;
            case APU_DMC_CTRL:
                set_dmc_ctrl(apu, value);
                break;
            case APU_DMC_DA:
                set_dmc_da(&apu->dmc, value);
                break;
            case APU_DMC_LEN:
                set_dmc_length(&apu->dmc, value);
                break;
            case FRAME_COUNTER:
                set_frame_counter_ctrl(apu, value);
                break;
            case APU_STATUS:
                set_status(apu, value);
                break;
            default:
                LOG(DEBUG, "Cannot write to register 0x%X", address);
                break;
        }
        return;
    }

    mem->mapper->write_ROM(mem->mapper, address, value);
}
uint8_t read_mem(Memory* mem, uint16_t address){
    if(address < RAM_END)
        return mem->RAM[address % RAM_SIZE];
    
    // resolve mirrored registers
    if(address < IO_REG_MIRRORED_END)
        address = 0x2000 + (address - 0x2000) % 0x8;

    // handle all IO registers
    if(address < IO_REG_END){
        PPU* ppu = &mem->emulator->ppu;
        switch (address) {
            case PPU_STATUS:
                return read_status(ppu);
            case OAM_DATA:
                return read_oam(ppu);
            case PPU_DATA:
                return read_ppu(ppu);
            case JOY1:
                return read_joypad(&mem->joy1);
            case JOY2:
                return read_joypad(&mem->joy2);
            case APU_STATUS:
                return read_apu_status(&mem->emulator->apu);
            default:
                LOG(DEBUG, "Cannot read from register 0x%X", address);
                return 0;
        }
    }

    return mem->mapper->read_ROM(mem->mapper, address);
}
