# HISSTools_Library

A C++ Library for DSP 

## cmake system

Windows programs using this library should define the `NOMINMAX` and `_USE_MATH_DEFINES` macros in their build system

```sh
mkdir build && cd build
# build library
cmake .. -DCMAKE_INSTALL_PREFIX=<absolute/install/parent/dir> 
         -DBUILD_TESTS=<OFF|ON> 
         -DBUILD_TEST=$PWD/../test/<specific_test_to_build.cpp>
# install library CMAKE_INSTALL_PREFIX/include/hisstools
cmake --build . --target install
# build specific test, or leave out --target option to build all tests
cmake --build . --target <specific_test_built>
# run test if built, or leave out -R option to run all tests
ctest --verbose --build-config Debug -R specific_test_built
```