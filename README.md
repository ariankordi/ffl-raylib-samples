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
Don't have good instructions right now. Sorry.

* You will need to [build FFL (my fork) as a library](https://github.com/ariankordi/ffl?tab=readme-ov-file#building).

* Link it either statically or dynamically, alongside raylib, with the samples.

* They all expect FFLResHigh.dat in the current working directory, and _note that they will not be very clear if they can't find that, it will still work anyway!!!!!_

#### Build options

* Include dirs:
  - Raylib
  - I add RIO includes because it has GLAD but not actually sure if that's necessary
  - FFL
* Defs:
  - PLATFORM_DESKTOP
  - GRAPHICS_API_USE_OPENGL33
  - FFL_USE_TEXTURE_CALLBACK
  - SUPPORT_TRACELOG (optional)
* Link:
  - raylib, lGL/lGLESv2
  - zlib
  - lm
  - lsupc++ (**for FFL/RIO use of C++**)

Commands used by me:
```
# basic
gcc -g3 -Wall -Wextra -Wno-c23-extensions \
-Irio/include -Iraylib -Iffl/include \
ffl_raylib_shader_basic.c \
-Lffl/build/ -lffl -lsupc++ -lz -lGL -lraylib -lm \
-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 -DSUPPORT_TRACELOG \
-o ffl_raylib_shader_basic

# fflshader (mostly same as above)
gcc -g3 -Wall -Wextra -Wno-c23-extensions \
-Irio/include -Iraylib -Iffl/include \
ffl_raylib_shader_fflshader.c \
-Lffl/build/ -lffl -lsupc++ -lz -lGL -lraylib -lm \
-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 -DSUPPORT_TRACELOG \
-o ffl_raylib_shader_fflshader

# Emscripten (JUST pasting from my history, no guarantees or instructions for this at aALLLLLLLL)
emcc -DFFL_USE_TEXTURE_CALLBACK -DNO_MODELS_FOR_TEST -g3 -Wall \
-Wno-c23-extensions -include GLES2/gl2.h -I/home/arian/Downloads/build/raylib/src -I/home/arian/Downloads/ffl/include \
ffl_raylib_shader_fflshader.c ~/Downloads/build/ffl-build-glad/rio-ffl-glad-es2-em.a /home/arian/Downloads/build/raylib/src/libraylib.a \
-lm -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -DSUPPORT_TRACELOG \
-sASYNCIFY -sEXPORTED_RUNTIME_METHODS=ccall -sUSE_GLFW=3 -sUSE_ZLIB=1 -sALLOW_MEMORY_GROWTH \
--preload-file FFLResHigh.dat -o ffl_raylib_shader_fflshader.html
```

## TODO
* Need to set up CMake for this.
  - Project template: https://github.com/raysan5/raylib/blob/master/projects/CMake/CMakeLists.txt
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
