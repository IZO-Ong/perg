#!/bin/bash
set -e

TARGET="/usr/local/bin/perg"

if [ -f "$TARGET" ]; then
    echo "Removing $TARGET..."
    sudo rm "$TARGET"
    echo "Uninstall successful!"
else
    echo "Error: perg is not found in $TARGET"
    exit 1
fi