#!/bin/bash
set -e

mkdir -p build
cd build

echo "Configuring..."
cmake ..

echo "Building..."
make

echo "Installing to /usr/local/bin..."
sudo cp perg /usr/local/bin/

echo "Success! Try running: perg --help"