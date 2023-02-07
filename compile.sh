#!/bin/sh
cmake \
    -DCMAKE_INSTALL_PREFIX=$HOME/nestdaq \
    -DCMAKE_PREFIX_PATH=$HOME/nestdaq \
    -DCMAKE_CXX_STANDARD=17 \
    -B ./build \
    -S .
#cd build
#make -j8
#make install
