#!/bin/bash
set -e

INSTALL_DIR="/usr/local/bin"
BINARY_NAME="perg"
MANIFEST_PATH="build/install_manifest.txt"

if [[ "$1" == "--uninstall" ]]; then
    if [ -f "$MANIFEST_PATH" ]; then
        echo "Found CMake install manifest. Removing files..."
        sudo xargs rm -f < "$MANIFEST_PATH"
        echo "Success! All installed files removed."
    else
        echo "Manifest not found. Attempting manual removal of $BINARY_NAME..."
        sudo rm -f "$INSTALL_DIR/$BINARY_NAME"
        echo "Done."
    fi
    exit 0
fi

mkdir -p build
cd build

echo "Configuring with CMake..."
cmake ..

echo "Building $BINARY_NAME..."
make -j$(nproc)

echo "Installing to $INSTALL_DIR..."
sudo cp "$BINARY_NAME" "$INSTALL_DIR/"
echo "$INSTALL_DIR/$BINARY_NAME" > install_manifest.txt

echo "------------------------------------------------"
echo "Success! Try running: $BINARY_NAME --help"
echo "To uninstall later, run: ./install.sh --uninstall"