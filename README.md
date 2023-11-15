# nestdaq-user-impl


User implementation for	nestdaq	framework.
Process	including so called Sampler, Sink, Filter etc are prepared.

## Pre-requirement

Installation of	nestdaq	and its	dependent libraries is required.
In addition, a mini booking tool UHBOOK and CERN ROOT (newer versions v6.28.06 etc.) are also required.
Please refer to the [nestdaq repository](https://github.com/spadi-alliance/nestdaq) for the system	and dependencies requirement.

## Installation

In this instruction, we assume that the nestdaq is installed in the dierctory $HOME/nestdaq. The nestdaq-user-impl is also installed in the same directory.
```
cd $HOME/nestdaq/src
git clone https://github.com/spadi-alliance/uhbook
git clone https://github.com/spadi-alliance/nestdaq-user-impl
cd $HOME/nestdaq/src/nestdaq-user-impl
cmake  -DCMAKE_INSTALL_PREFIX=$HOME/nestdaq \
               -DCMAKE_PREFIX_PATH="$HOME/nestdaq;$HOME/nestdaq/src/uhbook”\
               -B ./build     -S .
cd build
make
make install
```
