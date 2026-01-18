#!/bin/bash
# A script to build and install perg
mkdir -p build
cd build
cmake ..
make
sudo cp perg /usr/local/bin/
echo "perg has been installed! Try running: perg --help"