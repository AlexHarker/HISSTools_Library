# HISSTools_Library

A C++ Library for DSP 

## cmake system

```sh
# build
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=<install parent dir of {include,lib,share}>
# install
cmake --build . --target install
```