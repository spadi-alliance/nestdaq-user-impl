#!/bin/sh
cmake -DCMAKE_PREFIX_PATH=$HOME/nestdaq  -DCMAKE_INSTALL_PREFIX=. -DCMAKE_CXX_STANDARD=17 -B build -S .
#cmake -DCMAKE_PREFIX_PATH=$HOME/nestdaq  -DCMAKE_INSTALL_PREFIX=$HOME/nestdaq -DCMAKE_CXX_STANDARD=17 -B build -S .
#cmake -DCMAKE_PREFIX_PATH=$HOME/nestdaq/  -DCMAKE_INSTALL_PREFIX=$HOME/run/local -DCMAKE_CXX_STANDARD=17 -B build -S .
#cd build
#make
#make install
