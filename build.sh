#!/usr/bin/env bash
set -e

# fixes "Authorization required, but no authorization protocol specified" error
# echo "xhost si:localuser:root" >> ~/.bashrc

mkdir -p build
cd build
cmake -D CMAKE_C_COMPILER=clang -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_EXPORT_COMPILE_COMMANDS=1 ..
cp compile_commands.json ..

make -j $(nproc) && make Shaders
cd ..

mkdir -p bin
cp ./build/src/game/game ./bin/
cp ./build/src/editor/editor ./bin/

echo "Build finished!"
