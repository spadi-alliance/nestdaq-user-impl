#!/bin/sh
cmake  -DCMAKE_INSTALL_PREFIX=./install \
       -DCMAKE_PREFIX_PATH="$HOME/spadi;$HOME/spadi/src/uhbook;$HOME/spadi/src/EDM4hep/install;$HOME/spadi/src/podio/install" \
       -B ./build -S .
#cd build
#make
#make install
