# ANDAMAN DOSER FIRMWARE

the firmware for the Andaman Doser

## overview

This repo is still a little bit chaotic and unpolished, this is a one-woman operation and im trying to prototype quick so sometimes readability suffers. Sorry about that.

This repo only contains the .c and .h files for the firmware, you have to set up your own development environment and config files.


## file structure

The root directory contains the main file (with the super loop) and the some generic prototypes and definitions. Each specific function is grouped in its respective folders (stepper code is in stepper/ etc.). The code in these directories are intended to be relatively stand-alone (but still require prot.h and sometimes each others header files) and provide a simple interface for other modules to use. It is intended such that you could easily drop-in replace your own code if you use different ICs/APIs/whatever.


## contributing

There are many ways to contribute, you can of course commit your own code to the repo (please do), but it also really helps if you submit issues for bugs or for code improvements (readability and suchlike). The more eyes (and fingers) on this, the better. 