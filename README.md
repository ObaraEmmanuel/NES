# An NES emulator written in pure C

Here lies an NES emulator I wrote to learn about emulation and generally
sharpen my understanding of the C language. Don't let this fool you though.
It is a pretty decent emulator with full 6502 instruction set (official, 
unofficial and most illegal opcodes) to say the least. It is also an 
impressive NSF player.

## What is implemented

Below shows the features that have been implemented. This is obviously
subject to change as I will continue to implement features where I can 
over time. Any help is welcome.

* CPU (6502)
    - [x] Official opcodes
    - [x] Unofficial opcodes
    - [x] Cycle accuracy (official & unofficial)
    - [x] Dummy reads/writes
    - [ ] BCD arithmetic (Not needed by NES anyway)
* Memory
    - [ ] Battery backed (persistent) save RAM
    - [x] Open-bus
* PPU (Picture Processing Unit)
    - [x] Open-bus
    - [x] NTSC
    - [x] PAL
    - [ ] Dendy
* APU (Audio Processing Unit)
    - [x] Pulse channel
    - [x] Triangle channel
    - [x] Noise channel
    - [x] Delta Modulation Channel
* Gaming input
    - [x] Keyboard input
    - [ ] Keyboard (multiplayer)
    - [ ] Original NES controller
    - [x] Gamepad controller (Multiplayer)
    - [x] Turbo keys
    - [x] Custom Touch controller (Android)
* Formats
    - [x] iNES
    - [x] NES2.0
    - [x] NSF
    - [x] NSFe
    - [x] NSF2
    - [ ] UNIF
    - [ ] FDS
    - [ ] IPS
* Mappers
    - [x] \#0   NROM
    - [x] \#1   MMC1
    - [x] \#2   UxROM
    - [x] \#3   CNROM
    - [x] \#4   MMC3
    - [ ] \#5   MMC5
    - [x] \#7   AxROM
    - [x] \#11  Color Dreams
    - [x] \#66  GNROM
* Game genie (for cheat codes)
    
## What can be played

I haven't really taken time to do a survey but based on mappers implemented 
we can put the estimate at 90% of NES games. This is assuming none of them are 
using a quirk that I haven't gotten round to implementing. Games using 
mid-scanline trickery may not work as expected unless I find time to iron
out timing issues in the PPU. Below are a few demos:

_Contra_

![contra](resources/contra.png)

_Metal gear_

![metal gear](resources/metalgear.png)

_Legend of Zelda_

![zelda](resources/zelda.png)

_1943: Battle of Midway_

![1943](resources/1943.png)

_NSF Player: Ninja Gaiden_

![Ninja Gaiden NSF](resources/ninja-gaiden-nsf.png)

_NSFe Player: Castlevania 3_

![Castlevania 3 NSF](resources/castlevania-3-nsfe.png)

_Android: Ninja Gaiden_

![Ninja Gaiden Android](resources/ninja-gaiden-android.png)

## Controller setup
Xbox and Playstation controllers have not been tested on the emulator and are not guaranteed to work
as shown here.

| **Key** | **Keyboard** | **Playstation**   | **Xbox**          |
|---------|--------------|-------------------|-------------------|
 | Start   | Enter        | Start             | Menu              |
 | Select  | Shift        | Select            | View              |
 | A       | J            | ▢                 | Y                 |
 | B       | K            | ◯                 | B                 |
 | Turbo A | H            | △                 | A                 |
 | Turbo B | B            | X                 | X                 |
 | up      | Up           | D-pad/stick Up    | D-pad/stick Up    |
 | down    | Down         | D-pad/stick Down  | D-pad/stick Down  |
 | left    | Left         | D-pad/stick Left  | D-pad/stick Left  |
 | right   | Right        | D-pad/stick Right | D-pad/stick Right |

## Compiling

Compiling the emulator requires:
* a C11 (or higher) compiler e.g. gcc
* cmake
* make (linux), msvc nmake (windows)

Assuming have the above requirements installed on your system,
Acquire and build the source as follows:

```shell
$ git clone --recurse-submodules https://github.com/obaraemmanuel/NES
$ cd NES
$ mkdir build
$ cmake . -B build
$ cd build
$ make
```

You can now run the built emulator

```shell
$ ./nes ~/nes/roms/Contra.nes
```

To run with *Game Genie* enabled provide path to the original game genie ROM as an extra argument
```shell
$ ./nes ~/nes/roms/Contra.nes ~/nes/roms/game_genie.nes
```