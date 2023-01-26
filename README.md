# nestdaq-user-impl


User implementation for	nestdaq	framework.
Process	including so called Sampler, Sink, Filter etc are prepared.

## Pre-requirement

Installation of	nestdaq	and its	dependent libraries is required.
The system requirement is same as the nestdaq framewwork.

Please refer to the [nestdaq repository](https://github.com/spadi-alliance/nestdaq) for the system	and dependencies requirement.


## Installation

The environmental variables for	compilation of the nestdaq framework are necessary.

git clone https://github.com/spadi-alliance/nestdaq-user-impl.git
cd nestdaq-user-impl
cmake -DCMAKE_INSTALL_PREFIX=somewhere_in_PATH -B build -S . 
cd build
make 
make install

