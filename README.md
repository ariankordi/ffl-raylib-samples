# Deprecated Repo (2026)
I'm archiving this repo because of breaking changes in [my FFL fork](https://github.com/ariankordi/ffl), and because I am soon about to publish a library for Mii rendering with Raylib in a more modular way.

The last version of my FFL fork tested to work with this is here: https://github.com/ariankordi/ffl/commit/a784e6c7553b67ce7a4619ad3349676103983534

If you're here for the Mii body model files, what I really want to do is [make a script to automatically extract them](https://github.com/ariankordi/FFL.js/issues/4) and then have a repo dedicated to stuff like that. Until then, the ones here should work fine. 

# FFL Raylib Samples
Samples for rendering Miis using FFL, the Wii U Mii renderer, and raylib.

[raylib](https://github.com/raysan5/raylib) is a simple game/rendering framework using OpenGL. The samples use OpenGL calls, so this should help seeing how to use FFL with any other OpenGL framework or context.

The samples should also build for the WebGL target on OpenGL ES 2.0, demonstrating FFL will work there.

This repo contains:
* ffl_raylib_shader_basic: Renders a spinning Mii head with no lighting.
* ffl_raylib_shader_fflshader: Spinning Mii head using the FFLShader/Wii U Mii shader.
  - It can also show the head on its body with an animation.
  - ... and as a bonus, cat ears.

### Screenshots

* ffl_raylib_shader_basic

<img src="https://github.com/user-attachments/assets/694be992-2566-44e1-b266-560261563853" height="500">

* ffl_raylib_shader_fflshader

<img src="https://github.com/user-attachments/assets/28901ea2-4e20-49c6-9fdb-a18cbd67e980" height="500">

### On WebGL 1.0:

<img src="https://github.com/user-attachments/assets/f6042c2e-4216-4c97-be98-e74b8c7d95f0" height="500">


* ffl_raylib_shader_fflshader with body

<img src="https://github.com/user-attachments/assets/eaf4c7d2-0d5c-4afb-a762-4eac0a3bc36d" height="500">

## Building

The two requirements are raylib and FFL. CMake is used for building.

For simplicity, FFL-Testing is included as a submodule as it has all dependendcies needed to build FFL.

1. Clone and install raylib.

```sh
git clone https://github.com/raysan5/raylib && cd raylib
cmake -S . -B build
cmake --build build
sudo cmake --build install # Optional, read below.
```
Alternatively, if you don't want to install, pass `-Draylib_DIR=/path/to/raylib/build/raylib` to the cmake command for this repo.

2. Make sure FFL-Testing submodule is cloned.

You can run `git submodule update --init --recursive` if you didn't already clone recursively.

3. Build with CMake.

```sh
cmake -S . --build build # Specify -Draylib_DIR if you have to.
cmake --build build
```

This will build FFL as a static library and then the samples afterwards.

To build for the web, build raylib for the web first (just use `emcmake`), then when building this, specify: `-DCMAKE_TOOLCHAIN_FILE=/path/to/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`.

4. Run the binaries built in the current directory.
Make sure you have FFLResHigh.dat, or else either the Mii head won't render or it'll crash.

## TODO
* Add .clang-format
  - Really this is needed for alllllllll projects: ffl, rio, FFL-Testing.....
* Look at common code (e.g. texture callback) and consider putting it in a shared file or header
* Remove OpenGL calls and use raw rlgl calls, which abstract to OpenGL 1.1/2.1/3.3/ES2
  - The repo originally intended to show off FFL being used with raw OpenGL, but  I'll link to the older versions if someone still wants that.
* Read in an ffsd from argv for testing
  - More testing is needed for other Miis to begin with....
* Break out fflshader into a version with and without body

#### less essential
* Fix all Wextra/linting flaws
* Add mipmap support to fflshader version
* ig it would be nice to have a sample with [raygui](https://github.com/raysan5/raygui)
  - just... play and add stuff!!!! idk.

#### build mii editor to work on michaelsoft xp

* you need mingw
* replace `mingw-w64-i686` with `mingw-w64-x86_64` if you need to
1. `wget https://raw.githubusercontent.com/eclipse-cyclonedds/cyclonedds/ad52f7b7890ee1b87528d55c3014752c05b8e92d/ports/mingw-w64/mingw-w64-i686.cmake`
2. `cmake -DCMAKE_TOOLCHAIN_FILE=mingw-w64-i686.cmake -B build -S . -DUSE_LATEST_RAYLIB_RGFW=ON -DFFL_USE_EM_INFLATE=ON -DCMAKE_EXE_LINKER_FLAGS="-mcrtdll=msvcrt-os -mwindows" -DCMAKE_C_FLAGS="-DRGFW_NO_DWM -DRGFW_NO_DPI"`
3. `cmake --build build -j4`

but it needs opengl 3.3 so probably will not work on your typical xp machine, but certain nvidia gpus did support this on xp like the 9400m/ion
