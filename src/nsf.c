#include <stdlib.h>
#include <string.h>

#include "nsf.h"
#include "utils.h"
#include "emulator.h"

#define PRG_ROM_SIZE 0x8000
#define PRG_RAM_SIZE 0x2000
#define MAX_SILENCE 150 // frames
#define RESTART_THRESHOLD 3000 // 3 sec

static void NSF_NMI_hook(c6502* ctx, int phase);

static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper*, uint16_t);
static void write_CHR(Mapper*, uint16_t, uint8_t);
static uint8_t read_ROM(Mapper*, uint16_t);
static void write_ROM(Mapper*, uint16_t, uint8_t);

static void log_nsf_flags(uint8_t flags);
static void log_sound_chip_flags(uint8_t flags);
static void read_text_stream(char** list, const char* buf, size_t list_len, size_t buf_len, size_t max_str_len);
static int load_info_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf);
static int load_data_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf);
static int load_bank_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf);
static int load_rate_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf);
static int load_auth_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf);
static int load_time_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf);
static int load_fade_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf);
static int load_tlbl_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf);
static int load_text_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf);

static void log_nsf_flags(uint8_t flags) {
    LOG(DEBUG, "IRQ [%c] | Non returning init [%c] | No play [%c] | NSFe included [%c]",
        flags & NSF_IRQ ? 'Y' : 'N',
        flags & NSF_NON_RETURN_INIT ? 'Y' : 'N',
        flags & NSF_NO_PLAY_SR ? 'Y' : 'N',
        flags & NSF_REQ_NSFE_CHUNKS ? 'Y' : 'N'
    );
}

static void log_sound_chip_flags(uint8_t flags) {
    LOG(DEBUG, "VRC6 [%c] | VRC7 [%c] | FDS [%c] | MMC5 [%c] | Namco 163 [%c] | Sunsoft 5B [%c] | VT02+ [%c]",
        flags & BIT_0 ? 'Y' : 'N',
        flags & BIT_1 ? 'Y' : 'N',
        flags & BIT_2 ? 'Y' : 'N',
        flags & BIT_3 ? 'Y' : 'N',
        flags & BIT_4 ? 'Y' : 'N',
        flags & BIT_5 ? 'Y' : 'N',
        flags & BIT_6 ? 'Y' : 'N'
    );
}

void read_text_stream(char** list, const char* buf, size_t list_len, size_t buf_len, size_t max_str_len) {
    if(list == NULL || buf == NULL)
        return;
    size_t offset = 0, str = 0, idx = 0;
    while(offset < buf_len) {
        char c = buf[offset++];
        if(c == 0) {
            str++;
            if(str >= list_len)
                break;
            idx = 0;
        } else if(idx < max_str_len) {
            list[str][idx++] = c;
        } else {
            // string is too long
            while (c != 0 && offset < buf_len) {
                c = buf[offset++];
            }
            // add ellipsis at the end to show content is truncated
            strncpy(list[str] + max_str_len - 3, "...", 3);
            // reset to just before null byte if there is still stuff to be read
            if(offset < buf_len)
                offset--;
        }
    }
}

int load_info_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf) {
    const uint8_t* chunk = buf;
    if(len < 9) {
        LOG(ERROR, "INFO chunk too short");
        return -1;
    }
    NSF* nsf = mapper->NSF;
    nsf->load_addr = (chunk[1] << 8) | chunk[0];
    if(nsf->load_addr < 0x8000) {
        LOG(ERROR, "Load address ox%04x too low", nsf->load_addr);
        return -1;
    }

    nsf->init_addr = (chunk[3] << 8) | chunk[2];
    nsf->play_addr = (chunk[5] << 8) | chunk[4];

    if(chunk[0x6] & BIT_1) {
        // PAL/NTSC
        LOG(INFO, "ROM type: Dual Compatible (using NTSC)");
        mapper->type = NTSC;
    }else if(chunk[0x6] & BIT_0) {
        // PAL
        LOG(INFO, "ROM type: PAL");
        mapper->type = PAL;
    }else {
        // NTSC
        LOG(INFO, "ROM type: NTSC");
        mapper->type = NTSC;
    }

    if(mapper->type == PAL) {
        nsf->speed = 19997;
    }else {
        nsf->speed = 16666;
    }
    LOG(INFO, "Play speed: %.2f Hz", 1000000.0f / nsf->speed);

    if(chunk[0x7]) {
        log_sound_chip_flags(chunk[0x7]);
        LOG(WARN, "Requires extra sound chip support that is not available");
    }

    nsf->total_songs = chunk[8];
    if (len > 9)
        nsf->starting_song = nsf->current_song = chunk[9] + 1;
    else
        nsf->starting_song = nsf->current_song = 1;

    return 0;
}

int load_data_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf) {
    NSF* nsf = mapper->NSF;
    if(nsf->bank_switch) {
        uint16_t padding = nsf->load_addr & 0xfff;
        size_t prg_size = len + padding;
        if(prg_size % 0x1000) {
            // ensure size is a multiple of 4kb
            prg_size = (prg_size / 0x1000 + 1) * 0x1000;
        }
        mapper->PRG_banks = prg_size / 0X1000;
        LOG(INFO, "PRG banks: %llu", mapper->PRG_banks);
        mapper->PRG_ROM = malloc(prg_size);
        memset(mapper->PRG_ROM, 0, prg_size);
        memcpy(mapper->PRG_ROM + padding, buf, len);

        // compute bank pointers
        for(size_t i = 0; i < 8; i++) {
            mapper->write_ROM(mapper, 0x5ff8 + i, nsf->bank_init[i]);
        }

    } else {
        mapper->PRG_ROM = malloc(PRG_ROM_SIZE);
        memset(mapper->PRG_ROM, 0, PRG_ROM_SIZE);
        size_t read_len = MIN(len, 0x10000 - nsf->load_addr);
        memcpy(mapper->PRG_ROM + (nsf->load_addr - 0x8000), buf, read_len);
    }
    return 0;
}

int load_bank_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf) {
    NSF* nsf = mapper->NSF;
    nsf->bank_switch = 1;
    const uint8_t* bank_data = buf;
    uint32_t cap = len > 8 ? 8 : len;

    for(size_t i = 0; i < cap; i++) {
        nsf->bank_init[i] = bank_data[i];
    }
    return 0;
}

int load_rate_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf) {
    NSF* nsf = mapper->NSF;
    if(len < 2 || (len & 1 && len < 8)) {
        LOG(ERROR, "Invalid RATE chunk");
        return -1;
    }

    const uint8_t* chunk = buf;

    if(mapper->type == NTSC) {
        nsf->speed = (chunk[1] << 8) | chunk[0];
    }
    if(len > 2 && mapper->type == PAL) {
        nsf->speed = (chunk[3] << 8) | chunk[2];
    }
    if(len > 4 && mapper->type == DENDY) {
        nsf->speed = (chunk[5] << 8) | chunk[4];
    }
    return 0;
}

int load_auth_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf) {
    NSF* nsf = mapper->NSF;

    char* fields[4] = {
        nsf->song_name,
        nsf->artist,
        nsf->copyright,
        nsf->ripper
    };

    read_text_stream(fields, (char*)buf, 4, len, MAX_TEXT_FIELD_SIZE);
    LOG(INFO, "SONG_NAME: %s", nsf->song_name);
    LOG(INFO, "ARTIST: %s", nsf->artist);
    LOG(INFO, "COPYRIGHT: %s", nsf->copyright);
    LOG(INFO, "RIPPER: %s", nsf->ripper);
    return 0;
}

int load_time_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf) {
    NSF* nsf = mapper->NSF;

    nsf->times = malloc(4 * nsf->total_songs);
    memset(nsf->times, 0, 4 * nsf->total_songs);

    memcpy(nsf->times, buf, len);
    for(uint32_t i = len / 4; i < nsf->total_songs; i++) {
        nsf->times[i] = NSF_DEFAULT_TRACK_DUR;
    }
    return 0;
}

int load_fade_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf) {
    NSF* nsf = mapper->NSF;

    nsf->fade = malloc(4 * nsf->total_songs);
    memset(nsf->fade, 0, 4 * nsf->total_songs);
    memcpy(nsf->fade, buf, len);
    return 0;
}

int load_tlbl_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf) {
    NSF* nsf = mapper->NSF;

    nsf->tlbls = malloc(nsf->total_songs * sizeof(char*));
    memset(nsf->tlbls, 0, nsf->total_songs * sizeof(char*));

    for(int i = 0; i < nsf->total_songs; i++) {
        nsf->tlbls[i] = (char*)malloc(MAX_TRACK_NAME_SIZE + 1);
        memset(nsf->tlbls[i], 0, MAX_TRACK_NAME_SIZE + 1);
    }
    read_text_stream(nsf->tlbls, (char*)buf, nsf->total_songs, len, MAX_TRACK_NAME_SIZE);
    return 0;
}

int load_text_chunk(uint32_t len, Mapper* mapper, const uint8_t* buf) {
    LOG(INFO, "TEXT: \n%.*s \n", len, buf);
    return 0;
}

int load_nsfe(ROMData* rom_data, Mapper* mapper) {
    // PRG RAM
    mapper->PRG_RAM = malloc(PRG_RAM_SIZE);
    memset(mapper->PRG_RAM, 0, PRG_RAM_SIZE);

    // mapper R/W redirects
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    mapper->read_ROM = read_ROM;
    mapper->write_ROM = write_ROM;

    // skip the header
    int64_t offset = 4;

    mapper->is_nsf = 1;

    NSF* nsf = calloc(1, sizeof(NSF));
    mapper->NSF = nsf;
    nsf->version = 1;

    // defaults
    strncpy(nsf->song_name, "<?>", 3);
    strncpy(nsf->artist, "<?>", 3);
    strncpy(nsf->copyright, "<?>", 3);
    strncpy(nsf->ripper, "<?>", 3);

    uint32_t len;
    int has_info = 0;

    while(1) {
        char id[5];
        memcpy(&len, rom_data->rom + offset, 4);
        offset += 4;
        memset(id, 0, 5);
        memcpy(id, rom_data->rom + offset, 4);
        offset += 4;

        LOG(DEBUG, "Chunk: %s (%d)", id, len);
        int result = 0;

        if(strncmp(id, "INFO", 4) == 0) {
            result = load_info_chunk(len, mapper, rom_data->rom + offset);
            has_info = 1;
        } else if(strncmp(id, "DATA", 4) == 0) {
            if(!has_info) {
                LOG(ERROR, "Missing INFO chunk before DATA");
                return -1;
            }
            result = load_data_chunk(len, mapper, rom_data->rom + offset);
        } else if(strncmp(id, "BANK", 4) == 0) {
            if(!has_info) {
                LOG(ERROR, "Missing INFO chunk before BANK");
                return -1;
            }
            result = load_bank_chunk(len, mapper, rom_data->rom + offset);
        } else if (strncmp(id, "NEND", 4) == 0) {
            break;
        } else if (strncmp(id, "NSF2", 4) == 0) {
            memcpy(&nsf->flags, rom_data->rom + offset, len);
            nsf->version = 2;
            LOG(INFO, "Using NSF 2.0");
            log_nsf_flags(nsf->flags);
        } else if(strncmp(id, "RATE", 4) == 0) {
            result = load_rate_chunk(len, mapper, rom_data->rom + offset);
        } else if(strncmp(id, "VRC7", 4) == 0) {
            // Skip for now
        } else if(strncmp(id, "auth", 4) == 0) {
            result = load_auth_chunk(len, mapper, rom_data->rom + offset);
        } else if(strncmp(id, "time", 4) == 0) {
            if(has_info) {
                result = load_time_chunk(len, mapper, rom_data->rom + offset);
            }
        } else if(strncmp(id, "fade", 4) == 0) {
            if(has_info) {
                result = load_fade_chunk(len, mapper, rom_data->rom + offset);
            }
        } else if(strncmp(id, "tlbl", 4) == 0) {
            if(has_info) {
                result = load_tlbl_chunk(len, mapper, rom_data->rom + offset);
            }
        } else if(strncmp(id, "text", 4) == 0) {
            result = load_text_chunk(len, mapper, rom_data->rom + offset);
        } else if(id[0] > 65 && id[0] < 90) {
            LOG(ERROR, "Required chunk %s not implemented", id);
            return -1;
        } else {
            LOG(DEBUG, "Skipping chunk %s", id);
        }
        if (result < 0)
            return result;

        // move to start of next chunk
        offset += len;
    }
    LOG(DEBUG, "Bank switching: %s", nsf->bank_switch ? "ON": "OFF");

    if (nsf->flags & NSF_IRQ) {
        // initialize IRQ vector with initial value at 0xfffe-0xffff if IRQ enabled
        nsf->IRQ_vector = read_PRG(mapper, 0xfffe) | read_PRG(mapper, 0xffff) << 8;
    }
    return 0;
}

int load_nsf(ROMData* rom_data, Mapper* mapper) {
    uint8_t* const header = rom_data->rom;
    size_t offset = NSF_HEADER_SIZE;

    mapper->is_nsf = 1;

    NSF* nsf = calloc(1, sizeof(NSF));
    mapper->NSF = nsf;

    nsf->version = header[5];
    nsf->total_songs = header[6];
    nsf->starting_song = nsf->current_song = header[7];

    nsf->load_addr = (header[9] << 8) | header[8];
    if(nsf->load_addr < 0x8000) {
        LOG(ERROR, "Load address ox%04x too low", nsf->load_addr);
        return -1;
    }

    nsf->init_addr = (header[0xb] << 8) | header[0xa];
    nsf->play_addr = (header[0xd] << 8) | header[0xc];

    strncpy(nsf->song_name, (char*)(header + 0xe), TEXT_FIELD_SIZE);
    strncpy(nsf->artist, (char*)(header + 0x2e), TEXT_FIELD_SIZE);
    strncpy(nsf->copyright, (char*)(header + 0x4e), TEXT_FIELD_SIZE);

    LOG(INFO, "SONG_NAME: %s", nsf->song_name);
    LOG(INFO, "ARTIST: %s", nsf->artist);
    LOG(INFO, "COPYRIGHT: %s", nsf->copyright);

    if(header[0x7a] & BIT_1) {
        // PAL/NTSC
        LOG(INFO, "ROM type: Dual Compatible (using NTSC)");
        mapper->type = NTSC;
    }else if(header[0x7a] & BIT_0) {
        // PAL
        LOG(INFO, "ROM type: PAL");
        mapper->type = PAL;
    }else {
        // NTSC
        LOG(INFO, "ROM type: NTSC");
        mapper->type = NTSC;
    }

    if(mapper->type == PAL) {
        nsf->speed = (header[0x79] << 8) | header[0x78];
    }else {
        nsf->speed = (header[0x6f] << 8) | header[0x6e];
    }
    LOG(INFO, "Play speed: %.2f Hz", 1000000.0f / nsf->speed);

    if(header[0x7b]) {
        log_sound_chip_flags(header[0x7b]);
        LOG(WARN, "Requires extra sound chip support that is not available");
    }

    nsf->flags = 0;
    if (nsf->version == NSF2) {
        nsf->flags = header[0x7c];
        LOG(INFO, "Using NSF 2.0");
        log_nsf_flags(nsf->flags);
    }

    size_t data_len = (header[0x7f] << 16) | (header[0x7e] << 8) | header[0x7d];

    size_t size = rom_data->rom_size;

    size -= NSF_HEADER_SIZE;
    if (data_len > size) {
        LOG(ERROR, "Error reading ROM, Invalid length");
        return -1;
    }
    if (!data_len)
        data_len = size;
    size_t metadata_len = size - data_len;


    LOG(DEBUG, "Program data length: %llu", data_len);
    LOG(DEBUG, "Metadata length: %llu", metadata_len);

    nsf->bank_switch = 0;
    for(size_t i = 0x70; i < 0x78; i++) {
        if(header[i] != 0) {
            nsf->bank_switch = 1;
            break;
        }
    }
    LOG(DEBUG, "Bank switching: %s", nsf->bank_switch ? "ON": "OFF");

    // PRG RAM
    mapper->PRG_RAM = malloc(PRG_RAM_SIZE);
    memset(mapper->PRG_RAM, 0, PRG_RAM_SIZE);

    // mapper R/W redirects
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    mapper->read_ROM = read_ROM;
    mapper->write_ROM = write_ROM;


    if(nsf->bank_switch) {
        uint16_t padding = nsf->load_addr & 0xfff;
        size_t prg_size = data_len + padding;
        if(prg_size % 0x1000) {
            // ensure size is a multiple of 4kb
            prg_size = (prg_size / 0x1000 + 1) * 0x1000;

        }
        mapper->PRG_banks = prg_size / 0X1000;
        LOG(DEBUG, "PRG banks: %llu", mapper->PRG_banks);
        mapper->PRG_ROM = malloc(prg_size);
        memset(mapper->PRG_ROM, 0, prg_size);
        memcpy(mapper->PRG_ROM + padding, rom_data->rom + offset, data_len);
        offset += data_len;

        // init banks
        for(size_t i =0; i < 8; i++) {
            mapper->write_ROM(mapper, 0x5ff8 + i, header[0x70+i]);
            nsf->bank_init[i] = header[0x70+i];
        }

    } else {
        mapper->PRG_ROM = malloc(PRG_ROM_SIZE);
        memset(mapper->PRG_ROM, 0, PRG_ROM_SIZE);
        size_t read_len = MIN(data_len, 0x10000 - nsf->load_addr);
        memcpy(mapper->PRG_ROM + (nsf->load_addr - 0x8000), rom_data->rom + offset, read_len);
        offset += read_len;
    }

    if (nsf->version == 2 && metadata_len > 0) {
        offset = NSF_HEADER_SIZE + data_len;
        uint32_t len = 0;
        while (offset < size) {
            char id[5];
            memcpy(&len, rom_data->rom + offset, 4);
            offset += 4;
            memset(id, 0, 5);
            memcpy(id, rom_data->rom + offset, 4);
            offset += 4;

            LOG(DEBUG, "Chunk: %s (%d)", id, len);
            int result = 0;

            if (strncmp(id, "NEND", 4) == 0) {
                break;
            }

            if(strncmp(id, "INFO", 4) == 0 || strncmp(id, "DATA", 4) == 0 || strncmp(id, "BANK", 4) == 0 || strncmp(id, "NSF2", 4) == 0) {
                // Skip
            } else if(strncmp(id, "RATE", 4) == 0) {
                result = load_rate_chunk(len, mapper, rom_data->rom + offset);
            } else if(strncmp(id, "VRC7", 4) == 0) {
                // Skip for now
            } else if(strncmp(id, "auth", 4) == 0) {
                result = load_auth_chunk(len, mapper, rom_data->rom + offset);
            } else if(strncmp(id, "time", 4) == 0) {
                result = load_time_chunk(len, mapper, rom_data->rom + offset);
            } else if(strncmp(id, "fade", 4) == 0) {
                result = load_fade_chunk(len, mapper, rom_data->rom + offset);
            } else if(strncmp(id, "tlbl", 4) == 0) {
                result = load_tlbl_chunk(len, mapper, rom_data->rom + offset);
            } else if(strncmp(id, "text", 4) == 0) {
                result = load_text_chunk(len, mapper, rom_data->rom + offset);
            } else if(id[0] > 65 && id[0] < 90) {
                LOG(ERROR, "Required chunk %s not implemented", id);
                return -1;
            } else {
                LOG(DEBUG, "Skipping chunk %s", id);
            }
            if (result < 0)
                return result;

            offset += len;
        }
    }

    if (nsf->flags & NSF_IRQ) {
        // initialize IRQ vector with initial value at 0xfffe-0xffff if IRQ enabled
        nsf->IRQ_vector = read_PRG(mapper, 0xfffe) | read_PRG(mapper, 0xffff) << 8;
    }
    return 0;
}

void nsf_execute(Emulator* emulator) {
    // called every cpu cycle if IRQ feature is enabled
    NSF* nsf = emulator->mapper.NSF;
    if (nsf->IRQ_status & BIT_0) {
        if (nsf->IRQ_counter == 0) {
            // IRQ assert
            nsf->IRQ_status |= BIT_7;
            interrupt(&emulator->cpu, MAPPER_IRQ);
            nsf->IRQ_counter = nsf->IRQ_counter_reload;
        }else {
            nsf->IRQ_counter--;
        }
    }
}

static int silent_frames = 0;

void nsf_tick_frame(struct Emulator* emulator) {
    if (MAX_SILENCE < 0)
        return;
    int silent = 1;
    const APU* apu = &emulator->apu;
    for(size_t i =0; i < AUDIO_BUFF_SIZE; i++) {
        if (apu->buff[i] != 0) {
            silent = 0;
            break;
        }
    }
    if(silent)
        silent_frames++;
    else
        silent_frames = 0;

    if(silent_frames > MAX_SILENCE) {
        next_song(emulator, emulator->mapper.NSF);
        silent_frames = 0;
    }
}

static uint8_t read_ROM(Mapper* mapper, uint16_t addr) {
    if(addr < 0x6000) {
        NSF* nsf = mapper->NSF;
        if (nsf->flags & NSF_IRQ) {
            switch (addr) {
                case 0x401B:
                    return nsf->IRQ_counter_reload & 0xff;
                case 0x401C:
                    return nsf->IRQ_counter_reload >> 8;
                case 0x401D: {
                    uint8_t flags = nsf->IRQ_status;
                    // Acknowledge IRQ
                    nsf->IRQ_status &= ~BIT_7;
                    interrupt_clear(&mapper->emulator->cpu, MAPPER_IRQ);
                    return flags;
                }
                default:
                    break;
            }
        }
        // open bus
        return mapper->emulator->mem.bus;
    }

    if(addr < 0x8000)
        return mapper->PRG_RAM[addr - 0x6000];

    return mapper->read_PRG(mapper, addr);
}

static void write_ROM(Mapper* mapper, uint16_t addr, uint8_t val) {
    if(addr < 0x6000) {
        NSF* nsf = mapper->NSF;
        if (nsf->flags & NSF_IRQ) {
            switch (addr) {
                case 0x401B:
                    nsf->IRQ_counter_reload &= ~0xff;
                    nsf->IRQ_counter_reload |= val;
                    break;
                case 0x401C:
                    nsf->IRQ_counter_reload &= ~0xff00;
                    nsf->IRQ_counter_reload |= val << 8;
                    break;
                case 0x401D:
                    nsf->IRQ_status |= val & BIT_0;
                    break;
                default:
                    break;
            }
        }
        if(addr > 0x5ff7) {
            nsf->bank_ptrs[addr - 0x5ff8] = mapper->PRG_ROM + val * 0x1000;
        }
    } else if(addr < 0x8000) {
        mapper->PRG_RAM[addr - 0x6000] = val;
    } else {
        mapper->write_PRG(mapper, addr, val);
    }
}

static uint8_t read_PRG(Mapper* mapper, uint16_t addr) {
    NSF* nsf = mapper->NSF;
    if (addr >= 0xfffe) {
        if (nsf->flags & NSF_IRQ)
            return addr == 0xfffe ? nsf->IRQ_vector & 0xff : nsf->IRQ_vector >> 8;
    }
    if(!nsf->bank_switch) {
        return mapper->PRG_ROM[addr - 0x8000];
    }
    size_t bank_index = (addr - 0x8000) / 0x1000;
    return *(nsf->bank_ptrs[bank_index] + (addr & 0xfff));
}

static void write_PRG(Mapper* mapper, uint16_t addr, uint8_t val) {
    NSF* nsf = mapper->NSF;
    if (!(nsf->flags & NSF_IRQ))
        return;
    if(addr == 0xfffe) {
        nsf->IRQ_vector &= ~0xff;
        nsf->IRQ_vector |= val;
    }else if(addr == 0xffff) {
        nsf->IRQ_vector &= ~0xff00;
        nsf->IRQ_vector |= val << 8;
    }
}

static uint8_t read_CHR(Mapper* mapper, uint16_t addr) {
    // CHR not required
    return 0;
}

static void write_CHR(Mapper* mapper, uint16_t addr, uint8_t val) {
    // CHR not required
}

void init_song(Emulator* emulator, size_t song_number) {
    memset(emulator->mem.RAM, 0, RAM_SIZE);
    init_cpu(emulator);
    set_cpu_mode(&emulator->cpu, CPU_WAIT_IRQ);
    emulator->apu.audio_start = 0;
    emulator->apu.sampler.index = 0;
    SDL_PauseAudio(emulator->g_ctx.audio_stream, 1);

    for(size_t i = 0; i < 14; i++) {
        write_mem(&emulator->mem, 0x4000 + i, 0);
    }
    write_mem(&emulator->mem, 0x4015, 0);
    write_mem(&emulator->mem, 0x4015, 0xf);
    write_mem(&emulator->mem, 0x4017, 0x40);

    NSF* nsf = emulator->mapper.NSF;
    if(nsf->bank_switch) {
        for(size_t i = 0; i < 8; i++) {
            write_mem(&emulator->mem, 0x5ff8 + i, nsf->bank_init[i]);
        }
    }
    emulator->cpu.ac = song_number > 0 ? song_number - 1: song_number;
    emulator->cpu.x = emulator->type == PAL? 1: 0;
    emulator->cpu.y = 0;
    nsf->initializing = 1;
    if(nsf->times != NULL) {
        nsf->tick_max = nsf->times[nsf->current_song == 0 ? 0 : nsf->current_song - 1];
        if(nsf->fade != NULL)
            nsf->tick_max += nsf->fade[nsf->current_song == 0 ? 0 : nsf->current_song - 1];
    }
    emulator->apu.volume = 1;
    nsf->init_num = 0; // first init call
    if (nsf->flags & NSF_NON_RETURN_INIT) {
        emulator->cpu.y = 0x80;
        emulator->cpu.NMI_hook = NSF_NMI_hook;
        run_cpu_subroutine(&emulator->cpu, nsf->init_addr);
    } else {
        run_cpu_subroutine(&emulator->cpu, nsf->init_addr);
    }

    if (nsf->flags & NSF_IRQ) {
        // set IRQ to inactive
        nsf->IRQ_status &= ~BIT_0;
        // SEI (inhibit interrupts)
        emulator->cpu.sr |= INTERRUPT;
    }
    LOG(DEBUG, "Initializing tune %d", nsf->current_song);
}

static void NSF_NMI_hook(c6502* ctx, int phase) {
    static uint8_t x, y, ac;
    if (phase == 0) {
        x = ctx->x;
        y = ctx->y;
        ac = ctx->ac;
        ctx->sub_address = ctx->emulator->mapper.NSF->play_addr;
    }else if(phase == 1) {
        ctx->x = x;
        ctx->y = y;
        ctx->ac = ac;
    }
}

void next_song(Emulator* emulator, NSF* nsf) {
    nsf->current_song = nsf->current_song >= nsf->total_songs ? 1 : nsf->current_song + 1;
    init_song(emulator, nsf->current_song);
}

void prev_song(Emulator* emulator, NSF* nsf) {
    if(nsf->tick < RESTART_THRESHOLD)
        nsf->current_song = nsf->current_song <= 1 ? nsf->total_songs : nsf->current_song - 1;
    init_song(emulator, nsf->current_song);
}

void free_NSF(NSF* nsf) {
    if(nsf == NULL)
        return;
    if(nsf->times != NULL)
        free(nsf->times);
    if(nsf->fade)
        free(nsf->fade);
    if(nsf->tlbls != NULL) {
        for(int i = 0; i < nsf->total_songs; i++) {
            if(nsf->tlbls[i] != NULL)
                free(nsf->tlbls[i]);
        }
        free(nsf->tlbls);
    }
    free(nsf);
}