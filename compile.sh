#!/bin/sh
cmake -DCMAKE_PREFIX_PATH=/opt/nestdaq/  -DCMAKE_INSTALL_PREFIX=./ -DCMAKE_CXX_STANDARD=17 -B build -S .
#cd build
#make
#make install
