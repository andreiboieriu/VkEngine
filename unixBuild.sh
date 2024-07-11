#!/usr/bin/env bash

CC=$(which clang)
CXX=$(which clang++)

mkdir -p build
cd build
cmake -D CMAKE_C_COMPILER=clang -D CMAKE_CXX_COMPILER=clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
# make && make Shaders && ./VulkanEngine
make -j $(nproc) && make Shaders
cd ..

mkdir -p bin
cp ./build/src/engine ./bin/
./bin/engine


