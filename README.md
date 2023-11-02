# gb_emulator

DMG Gameboy emulator written using `C++` and `SDL3`.

Implementation passes Blargg's CPU tests and instruction timings test. 
Tested games: Tetris and Super Mario Land. 

#### Building:
Firstly, clone the repo and download all submodules, then from the root directory run: 
```bash
git submodule update --init --recursive
mkdir build
cd build
cmake ..
cmake --build .
```
**Remember to copy built sdl dll to the appriopriate folder!**
Should compile under both Linux and Windows, altough it has only been tested on the WSL.

#### Known issues:
- only mbc 0 and 1 are supported.
- double height rendering is not supported.
- audio is barely implemented.
- cpu speed is synced to the refreshrate, so it will be faster on faster monitors.

#### Demo (audio is a bit loud!):

https://github.com/Szahu/gb_emulator/assets/53018959/e316cf85-1416-4900-8545-333bb4d39c3f

