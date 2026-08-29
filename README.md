## Overview
OpenGL perlin noise raymarching test.

Creates a dynamic 3D smoke-like object using raymarching in the fragment shader.
## Usage
You can rotate around the cube whilst holding RMB (orbit).<br>
You can also use the scroll wheel to zoom in and out.
## Building
This requries at least OpenGL 3.0 to run.

Use the following commands to clone the repository with the required submodules and to build using CMake:

```bash
$ git clone https://github.com/mahdialmusaad/raymarch-test --recurse-submodules
$ cd raymarch-test
$ cmake -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build --config=Release
```

Resulting executable can be found in the `build` directory.
