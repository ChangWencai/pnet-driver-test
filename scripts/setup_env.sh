#!/bin/bash
# setup_env.sh - Environment setup script for p-net driver development
# Based on document chapter 3: Preparation and environment configuration

set -e

echo "=== p-net Driver Development Environment Setup ==="
echo ""

# Check system info
echo "[1/5] Checking system compatibility..."
echo "  OS: $(uname -s) $(uname -r)"
echo "  Arch: $(uname -m)"
echo "  Kernel: $(uname -r)"
echo ""

# Check required tools
echo "[2/5] Checking build dependencies..."
DEPS=("gcc" "make" "git" "cmake")
MISSING=0

for dep in "${DEPS[@]}"; do
    if command -v "$dep" &> /dev/null; then
        echo "  ✓ $dep: $(command -v $dep)"
    else
        echo "  ✗ $dep: NOT FOUND"
        MISSING=$((MISSING + 1))
    fi
done

if [ $MISSING -gt 0 ]; then
    echo ""
    echo "  Missing $MISSING dependencies."
    echo "  Install with: sudo apt-get install build-essential cmake git"
    exit 1
fi
echo ""

# Check optional tools
echo "[3/5] Checking optional tools..."
OPT_DEPS=("iftop" "nethogs" "iptraf-ng" "valgrind")
for dep in "${OPT_DEPS[@]}"; do
    if command -v "$dep" &> /dev/null; then
        echo "  ✓ $dep"
    else
        echo "  - $dep: not installed (optional)"
    fi
done
echo ""

# Check network interfaces
echo "[4/5] Network interfaces:"
if command -v ip &> /dev/null; then
    ip -br addr 2>/dev/null || echo "  Unable to list interfaces"
else
    ifconfig -a 2>/dev/null | grep -E "^[a-z]" || echo "  Unable to list interfaces"
fi
echo ""

# Build test suite
echo "[5/5] Building test suite..."
cd "$(dirname "$0")/.."
make clean 2>/dev/null || true
make all
echo ""

echo "=== Setup Complete ==="
echo ""
echo "Run tests with: make test"
echo "Clone p-net source: git clone https://github.com/rtlabs/p-net.git"
