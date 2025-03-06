#!/usr/bin/env bash

# CC=$(which clang)
# CXX=$(which clang++)

mkdir -p build
cd build
# cmake -D CMAKE_C_COMPILER=clang -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_EXPORT_COMPILE_COMMANDS=1 ..
cmake -D CMAKE_C_COMPILER=clang -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_EXPORT_COMPILE_COMMANDS=1 ..
cp compile_commands.json ..

make -j $(nproc) && make Shaders
cd ..

mkdir -p bin
cp ./build/src/engine ./bin/
sudo XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR ./bin/engine
