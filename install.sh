#!/bin/bash
set -e

# Detect OS
OS_TYPE="$(uname -s)"
BINARY_NAME="perg"
INSTALL_DIR="/usr/local/bin"
MANIFEST_PATH="build/install_manifest.txt"

# Determine Parallel Job Count
if [[ "$OS_TYPE" == "Linux" ]]; then
    JOBS=$(nproc)
elif [[ "$OS_TYPE" == "Darwin" ]]; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=1 # Fallback for unknown environments
fi

# --- Uninstall Logic ---
if [[ "$1" == "--uninstall" ]]; then
    echo "Uninstalling $BINARY_NAME on $OS_TYPE..."
    if [ -f "$MANIFEST_PATH" ]; then
        sudo xargs rm -f < "$MANIFEST_PATH"
        echo "Success! Removed files listed in manifest."
    else
        sudo rm -f "$INSTALL_DIR/$BINARY_NAME"
        echo "Cleaned up $BINARY_NAME binary."
    fi
    exit 0
fi

# --- Build & Install Logic ---
mkdir -p build
cd build

echo "Configuring for $OS_TYPE..."
cmake ..

echo "Building with $JOBS cores..."
make -j"$JOBS"

echo "Installing to $INSTALL_DIR..."
sudo cp "$BINARY_NAME" "$INSTALL_DIR/"

# Create/Update Manifest
echo "$INSTALL_DIR/$BINARY_NAME" > install_manifest.txt

echo "------------------------------------------------"
echo "Success! $BINARY_NAME is now available on $OS_TYPE."
echo "Run: $BINARY_NAME --help"