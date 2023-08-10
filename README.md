# HISSTools_Library

A C++ Library for DSP 

## cmake system

```sh
mkdir build && cd build
# build library 
cmake .. -DCMAKE_BUILD_TYPE=<Debug|Release> -DCMAKE_INSTALL_PREFIX=<install parent dir of {include,lib,share}> -DBUILD_TESTING=<OFF|ON> -DBUILD_TEST=$PWD/../test/<specific_test_to_build_in_isolation>.cpp
# run tests if built
ctest --build-config <Debug|Release>
# install
cmake --build . --target install
```