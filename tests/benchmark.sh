#!/bin/bash

# 1. Determine script directory so it can be run from anywhere
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# Assuming 'build' and the 'perg' source are in the parent directory of 'tests'
# If benchmark.sh is in the root, change this to PROJECT_ROOT="$SCRIPT_DIR"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

# Configuration
TEST_DIR="perf_test_dir"
LARGE_FILE="$TEST_DIR/massive_data.txt"
PATTERN="target_match_pattern"

# Colors
CYAN='\033[0;36m'
GREEN='\033[0;32m'
NC='\033[0m'

echo -e "${CYAN}--- PERG Performance Benchmark ---${NC}"

# 2. Setup Data (Ensures a fresh file if run multiple times)
if [ -d "$TEST_DIR" ]; then
    echo "Cleaning old test data..."
    rm -rf "$TEST_DIR"
fi

echo "Generating 500MB test dataset..."
mkdir -p "$TEST_DIR"
# Generate ~500MB of random-looking text with specific patterns injected
for i in {1..500}; do
    base64 /dev/urandom | head -c 1000000 >> "$LARGE_FILE"
    echo "This line contains the $PATTERN" >> "$LARGE_FILE"
done
echo -e "${GREEN}Data generation complete.${NC}"

# 3. Build Release Version
echo "Building perg in Release mode..."
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. > /dev/null
make -j$(nproc) > /dev/null
cd "$PROJECT_ROOT"

# 4. Running Benchmarks
if command -v hyperfine &> /dev/null; then
    echo -e "\n${CYAN}Running Hyperfine Comparison...${NC}"
    # Note: sudo is required for dropping caches
    sync; sudo echo 3 | sudo tee /proc/sys/vm/drop_caches &> /dev/null || echo "Note: No sudo, testing Hot Cache performance."
    
    hyperfine --warmup 2 \
        "./build/perg -r '$PATTERN' $TEST_DIR" \
        "grep -r '$PATTERN' $TEST_DIR" \
        "rg '$PATTERN' $TEST_DIR"
else
    echo -e "\n${CYAN}Hyperfine not found. Using 'time' for a basic measurement:${NC}"
    echo -n "PERG: "
    time ./build/perg -r "$PATTERN" "$TEST_DIR" > /dev/null
    echo -n "GREP: "
    time grep -r "$PATTERN" "$TEST_DIR" > /dev/null
fi

# 5. Cleanup
echo -e "\n${CYAN}Cleaning up large test directory...${NC}"
rm -rf "$TEST_DIR"

echo -e "${GREEN}--- Benchmark Finished ---${NC}"