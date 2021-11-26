#include "emulator.h"
#include "utils.h"


int main(int argc, char *argv[]){

    struct Emulator emulator;
    init_emulator(&emulator, argc, argv);
    run_emulator(&emulator);

    LOG(INFO, "Play time %d min", (uint64_t)emulator.time_diff / 60000);
    LOG(INFO, "Frame rate: %.4f fps", (double)(emulator.ppu.frames * 1000) / emulator.time_diff);
    LOG(INFO, "CPU clock speed: %.4f MHz", ((double)emulator.cpu.t_cycles / (1000 * emulator.time_diff)));

    free_emulator(&emulator);

    return 0;
}