# HISSTools_Library

A C++ Library for DSP 

## cmake system

```sh
mkdir build && cd build
# build library 
cmake .. -DCMAKE_INSTALL_PREFIX=<absolute/install/parent/dir> -DBUILD_TESTS=<OFF|ON> -DBUILD_TEST=$PWD/../test/<specific_test_to_build>.cpp
# run tests if built
ctest --verbose --build-config Debug -R specific_test
# install
cmake --build . --target install
```