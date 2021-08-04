#include "emulator.h"
#include "utils.h"


int main(int argc, char *argv[]){

    struct Emulator emulator;
    init_emulator(&emulator, argc, argv);
    run_emulator(&emulator);

    uint32_t elapsed = emulator.m_end - emulator.m_start;

    LOG(INFO, "Play time %d min", elapsed / 60000);
    LOG(INFO, "Frame rate: %.4f fps", (double)(emulator.ppu.frames * 1000) / elapsed);
    LOG(INFO, "CPU clock speed: %.4f MHz", ((double)emulator.cpu.t_cycles / (1000 * elapsed)));

    free_emulator(&emulator);

    return 0;
}