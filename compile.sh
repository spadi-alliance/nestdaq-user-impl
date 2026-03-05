#!/bin/sh
cmake  -DCMAKE_INSTALL_PREFIX=./install \
       -DCMAKE_PREFIX_PATH="/home/work/NestDAQ/install;$HOME/exp/raris_ac-lgad_202603/install/podio;$HOME/exp/raris_ac-lgad_202603/install/EDM4hep;$HOME/exp/raris_ac-lgad_202603/install/EDM4eic" \
       -B ./build -S .
#cd build
#make
#make install
