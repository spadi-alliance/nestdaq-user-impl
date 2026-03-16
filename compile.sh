#!/bin/sh
cmake  -DCMAKE_INSTALL_PREFIX=./install \
       -DCMAKE_PREFIX_PATH="/home/work/NestDAQ/install;/home/work/NestDAQ/install/lib64/cmake;/home/work/NestDAQ/install/lib64/cmake/hiredis;/ope/local" \
       -B ./build -S .
#cd build
#make
#make install
