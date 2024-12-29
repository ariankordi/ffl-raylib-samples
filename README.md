# FFL Raylib Samples
Samples for rendering Miis using FFL, the Wii U Mii renderer, and raylib.

[raylib](https://github.com/raysan5/raylib) is a simple game/rendering framework using OpenGL. The samples use OpenGL calls, so this should help seeing how to use FFL with any other OpenGL framework or context.

The samples should also build for the WebGL target on OpenGL ES 2.0, demonstrating FFL will work there.

This repo contains:
* ffl-raylib-shader-basic: Renders a spinning Mii head with no lighting.
* ffl-raylib-shader-fflshader: Spinning Mii head using the FFLShader/Wii U Mii shader.
  - It can also show the head on its body with an animation.
  - ... and as a bonus, cat ears.

* "nofflgl" variants of each are made to work with, so that no OpenGL calls are made from FFL itself.
  - Should lead to a smaller binary at the cost of slightly more complexity.

### Screenshots

* ffl-raylib-shader-basic

(TBD)

* ffl-raylib-shader-fflshader

(TBD)

* ffl-raylib-shader-fflshader with body

(TBD)

## Building
Don't have good instructions right now. Sorry.

* You will need to [build FFL (my fork) as a library](https://github.com/ariankordi/ffl?tab=readme-ov-file#building).
  - The FFL_MODE depends:
  - `nofflgl` can just use FFL standalone, without passing FFL_MODE.
  - The standard variant assumes `opengl-33`, and WebGL/GLES assumes `opengl-es2`

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
  - FFL_USE_TEXTURE_CALLBACK (**needed for nofflgl**)
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
ffl-raylib-shader-basic.c \
-Lffl/build/ -lffl-opengl-33 -lsupc++ -lz -lGL -lraylib -lm \
-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 -DSUPPORT_TRACELOG \
-o ffl-raylib-shader-basic

# fflshader (mostly same as above)
gcc -g3 -Wall -Wextra -Wno-c23-extensions \
-Irio/include -Iraylib -Iffl/include \
ffl-raylib-shader-fflshader.c \
-Lffl/build/ -lffl-opengl-33 -lsupc++ -lz -lGL -lraylib -lm \
-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 -DSUPPORT_TRACELOG \
-o ffl-raylib-shader-fflshader


# For basic and fflshader nofflgl:
gcc -g3 -Wall -Wextra -Wno-c23-extensions \
-Irio/include -Iraylib -Iffl/include \
ffl-raylib-shader-fflshader-nofflgl.c \
-Lffl/build/ -lffl -lsupc++ -lz -lGL -lraylib -lm \
-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 -DSUPPORT_TRACELOG -DFFL_USE_TEXTURE_CALLBACK \
-o ffl-raylib-shader-fflshader-nofflgl

# Emscripten (JUST pasting from my history, no guarantees or instructions for this at aALLLLLLLL)
emcc -DFFL_USE_TEXTURE_CALLBACK -DNO_MODELS_FOR_TEST -g3 -Wall \
-Wno-c23-extensions -include GLES2/gl2.h -I/home/arian/Downloads/build/raylib/src -I/home/arian/Downloads/ffl/include \
ffl-raylib-shader-fflshader-nofflgl.c ~/Downloads/build/ffl-build-glad/rio-ffl-glad-es2-em.a /home/arian/Downloads/build/raylib/src/libraylib.a \
-lm -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -DSUPPORT_TRACELOG \
-sASYNCIFY -sEXPORTED_RUNTIME_METHODS=ccall -sUSE_GLFW=3 -sUSE_ZLIB=1 -sALLOW_MEMORY_GROWTH \
--preload-file FFLResHigh.dat -o ffl-raylib-shader-fflshader-nofflgl.html
```

## TODO
* Need to set up CMake for this.
  - Project template: https://github.com/raysan5/raylib/blob/master/projects/CMake/CMakeLists.txt
* Add .clang-format
  - Really this is needed for alllllllll projects: ffl, rio, FFL-Testing.....

* Unify the nofflgl and standard versions
  - Actually... you know you can build nofflgl and it will still work on regular gl target right...>???????
* Read in an ffsd from argv for testing
  - More testing is needed for other Miis to begin with....
* Break out fflshader into a version with and without body

#### less essential
* Fix all Wextra/linting flaws
* Add mipmap support to fflshader version
* ig it would be nice to have a sample with [raygui](https://github.com/raysan5/raygui)
  - just... play and add stuff!!!! idk.
