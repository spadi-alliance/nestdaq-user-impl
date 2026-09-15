#!/bin/sh
# GPU off: LogicFilterGpu builds CPU fallback
cmake \
  -DUSE_CUDA=OFF \
  -DCMAKE_PREFIX_PATH=$HOME/nestdaq \
  -DCMAKE_INSTALL_PREFIX=. \
  -DCMAKE_CXX_STANDARD=17 \
  -B build -S .
#cmake -DUSE_CUDA=OFF -DCMAKE_PREFIX_PATH=$HOME/nestdaq -DCMAKE_INSTALL_PREFIX=$HOME/nestdaq -DCMAKE_CXX_STANDARD=17 -B build -S .
#cmake -DUSE_CUDA=OFF -DCMAKE_PREFIX_PATH=$HOME/nestdaq/ -DCMAKE_INSTALL_PREFIX=$HOME/run/local -DCMAKE_CXX_STANDARD=17 -B build -S .
cmake --build build -j$(nproc)
#cmake --install build
