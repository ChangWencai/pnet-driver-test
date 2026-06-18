#!/bin/bash
# build_pnet.sh - Build p-net stack separately before main project
# Run this script BEFORE cmake .. -DUSE_REAL_PNET=ON

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PNET_DIR="$PROJECT_DIR/vendor/p-net"
PNET_BUILD="$PNET_DIR/build"

echo "=== Building p-net Profinet Stack ==="
echo ""

# Step 1: Init submodules
echo "[1/4] Initializing p-net submodules..."
cd "$PNET_DIR"
git submodule update --init --recursive
echo ""

# Step 2: Create build directory
echo "[2/4] Creating build directory..."
mkdir -p "$PNET_BUILD"
cd "$PNET_BUILD"
echo ""

# Step 3: Configure
echo "[3/4] Configuring p-net..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DPNET_MAX_PHYSICAL_PORTS=2 \
    -DPNET_MAX_DIRECTORYPATH_SIZE=240 \
    -DPNET_MAX_FILENAME_SIZE=30
echo ""

# Step 4: Build
echo "[4/4] Building p-net..."
make -j"$(nproc)"
echo ""

echo "=== p-net build complete ==="
echo ""
echo "Generated files:"
find "$PNET_BUILD" -name "pnet_options.h" -o -name "pnet_version.h" -o -name "pnal_config.h" -o -name "pnet_export.h" 2>/dev/null | while read f; do
    echo "  $f"
done
echo ""
echo "Static library:"
find "$PNET_BUILD" -name "*.a" 2>/dev/null | while read f; do
    echo "  $f"
done
echo ""
echo "Now build the main project:"
echo "  cd $PROJECT_DIR/build"
echo "  cmake .. -DUSE_REAL_PNET=ON -DCMAKE_BUILD_TYPE=Release"
echo "  make -j\$(nproc)"
