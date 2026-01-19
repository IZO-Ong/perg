#!/bin/bash
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

echo "Building perg..."
mkdir -p build && cd build
cmake .. > /dev/null
make > /dev/null
cd "$PROJECT_ROOT"

# output
GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "\nRunning Integration Tests..."

# input
INPUT_FILE="input.txt"
cat <<EOF > "$INPUT_FILE"
hello world
this is a test line with 2026 inside
multiple matches: test, test, and test
final line
EOF

test_no=1

# Test 1: Simple Exact Match
echo -n "Test $((test_no++)): Simple Exact Match... "
./build/perg "hello" "$INPUT_FILE" > test_output.txt
if grep -aq "hello world" test_output.txt; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 2: Regex Match
echo -n "Test $((test_no++)): Regex Match (finding 2026)... "
./build/perg "[0-9]{4}" "$INPUT_FILE" > test_output.txt
if grep -aq "2026" test_output.txt; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 3: Multiple Matches
echo -n "Test $((test_no++)): Multiple Matches on one line... "
./build/perg "test" "$INPUT_FILE" > test_output.txt
if [ $(grep -o "test" test_output.txt | wc -l) -eq 4 ]; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 4: Missing File Error
echo -n "Test $((test_no++)): Missing File Error Handling... "
if ./build/perg "test" non_existent_file.txt 2>/dev/null; then
    echo -e "${RED}FAIL (Should have errored)${NC}"
    exit 1
else
    echo -e "${GREEN}PASS${NC}"
fi

# Test 5: Help Function (-h)
echo -n "Test $((test_no++)): Help Function (-h)... "
if ./build/perg -h | grep -iq "usage"; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (Command failed or help text missing)${NC}"
    exit 1
fi

# Test 6: Help Function (--help)
echo -n "Test $((test_no++)): Help Function (--help)... "
if ./build/perg --help | grep -iq "usage"; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (Command failed or help text missing)${NC}"
    exit 1
fi

# Test 7: Count Flag (-c) Total Occurrences
echo -n "Test $((test_no++)): Count Flag (-c) Total Occurrences... "

actual_count=$(./build/perg -c "test" "$INPUT_FILE" | tr -d '[:space:]')

if [ "$actual_count" -eq 4 ]; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (Expected 4, got '$actual_count')${NC}"
    exit 1
fi

# Cleanup
rm "$INPUT_FILE" test_output.txt
echo -e "\n${CYAN}---------------------------------------${NC}"
echo -e "${GREEN}All $((test_no-1)) tests passed successfully!${NC}"