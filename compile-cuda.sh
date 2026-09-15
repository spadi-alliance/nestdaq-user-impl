#!/bin/sh
# GPU on: LogicFilterGpu uses CUDA (requires CUDA Toolkit)
cmake \
  -DUSE_CUDA=ON \
  -DCMAKE_PREFIX_PATH=$HOME/nestdaq \
  -DCMAKE_INSTALL_PREFIX=. \
  -DCMAKE_CXX_STANDARD=17 \
  -B build-cuda -S .
#cmake -DUSE_CUDA=ON -DCMAKE_PREFIX_PATH=$HOME/nestdaq -DCMAKE_INSTALL_PREFIX=$HOME/nestdaq -DCMAKE_CXX_STANDARD=17 -B build-cuda -S .
#cmake -DUSE_CUDA=ON -DCMAKE_PREFIX_PATH=$HOME/nestdaq/ -DCMAKE_INSTALL_PREFIX=$HOME/run/local -DCMAKE_CXX_STANDARD=17 -B build-cuda -S .
cmake --build build-cuda -j$(nproc)
#cmake --install build-cuda
