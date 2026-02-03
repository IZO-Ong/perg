#!/bin/bash
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

# Ensure cleanup happens on any exit (success or failure)
cleanup() {
    echo -e "\n${CYAN}Cleaning up temporary test files...${NC}"
    rm -rf "temp_test_dir" "input.txt" test_output.txt
}
trap cleanup EXIT

echo "Building perg..."
mkdir -p build && cd build
cmake .. > /dev/null
make > /dev/null
cd "$PROJECT_ROOT"

# output colors for the script itself
GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "\nRunning Integration Tests (Forcing --no-color for reliability)..."

# input setup
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
./build/perg --no-color "hello" "$INPUT_FILE" > test_output.txt
if grep -q "hello world" test_output.txt; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 2: Regex Match
echo -n "Test $((test_no++)): Regex Match (finding 2026)... "
./build/perg --no-color "[0-9]{4}" "$INPUT_FILE" > test_output.txt
if grep -q "2026" test_output.txt; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 3: Multiple Matches
echo -n "Test $((test_no++)): Multiple Matches on one line... "
./build/perg --no-color "test" "$INPUT_FILE" > test_output.txt
if [ $(grep -o "test" test_output.txt | wc -l) -eq 4 ]; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 4: Missing File Error
echo -n "Test $((test_no++)): Missing File Error Handling... "
if ./build/perg --no-color "test" non_existent_file.txt 2>/dev/null; then
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
actual_count=$(./build/perg --no-color -c "test" "$INPUT_FILE" | tr -d '[:space:]')
if [ "$actual_count" -eq 4 ]; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (Expected 4, got '$actual_count')${NC}"
    exit 1
fi

# Test 8: Complex Regex (Alternative/Grouping)
echo -n "Test $((test_no++)): Complex Regex (hello|final)... "
./build/perg --no-color "hello|final" "$INPUT_FILE" > test_output.txt
if [ $(grep -cE "hello world|final line" test_output.txt) -eq 2 ]; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    exit 1
fi

# Test 9: Multi-match Duplicate Check
echo -n "Test $((test_no++)): Literal Duplicate Line Check... "
./build/perg --no-color "test" "$INPUT_FILE" > test_output.txt
line_count=$(grep -c "test" test_output.txt)
if [ "$line_count" -eq 2 ]; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (Expected 2 unique lines, got $line_count)${NC}"
    exit 1
fi

# Test 10: Context Formatting
echo -n "Test $((test_no++)): Context Formatting (-C 1)... "
./build/perg --no-color -C 1 "this is a test line" "$INPUT_FILE" > test_output.txt
# Account for '1' followed by 3 spaces (padding 4)
if grep -qE "^1[[:space:]]+- hello world" test_output.txt && \
   grep -qE "^2[[:space:]]+: this is a test line" test_output.txt; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (Format mismatch in context)${NC}"
    cat -v test_output.txt
    exit 1
fi

# Folder setup
TEST_DIR="temp_test_dir"
mkdir -p "$TEST_DIR/sub1"
mkdir -p "$TEST_DIR/sub2"
echo "target in root" > "$TEST_DIR/file1.txt"
echo "target in sub1" > "$TEST_DIR/sub1/file2.txt"
echo "no match here" > "$TEST_DIR/sub2/file3.txt"

# Test 11: Recursive Directory Search
echo -n "Test $((test_no++)): Recursive Directory Search... "
./build/perg --no-color "target" "$TEST_DIR" > test_output.txt
if [ $(grep -c "target" test_output.txt) -eq 2 ]; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (Expected 2 matches)${NC}"
    exit 1
fi

# Test 12: Filename Prefixing Check
echo -n "Test $((test_no++)): Filename Prefixing Check... "
./build/perg --no-color "target" "$TEST_DIR" > test_output.txt
if grep -qE "file1.txt:1[[:space:]]+: target in root" test_output.txt && \
   grep -qE "sub1/file2.txt:1[[:space:]]+: target in sub1" test_output.txt; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (Format mismatch in directory prefix)${NC}"
    cat -v test_output.txt 
    exit 1
fi

# Test 13: Binary File Skipping
echo -n "Test $((test_no++)): Binary File Skipping Check... "
printf "match\0binary" > "$TEST_DIR/binary_file.dat"
./build/perg --no-color "match" "$TEST_DIR" > test_output.txt
if grep -q "binary_file.dat" test_output.txt; then
    echo -e "${RED}FAIL (Should have skipped binary file)${NC}"
    exit 1
else
    echo -e "${GREEN}PASS${NC}"
fi

# Test 14: Color Output Verification
echo -n "Test $((test_no++)): Color Output Verification (--color)... "
# Force color even though we are piping to a file
./build/perg --color "test" "$INPUT_FILE" > test_output.txt

if cat -v test_output.txt | grep -q "\^\[\["; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (No ANSI escape sequences found)${NC}"
    exit 1
fi

# Test 15: Explicit No-Color Verification (--no-color)
echo -n "Test $((test_no++)): Explicit No-Color Verification (--no-color)... "
./build/perg --no-color "test" "$INPUT_FILE" > test_output.txt
if cat -v test_output.txt | grep -q "\^\[\["; then
    echo -e "${RED}FAIL (ANSI codes found with --no-color)${NC}"
    exit 1
else
    echo -e "${GREEN}PASS${NC}"
fi

# Test 16: Default Behavior
echo -n "Test $((test_no++)): Default Color Behavior (Pipe Check)... "
./build/perg "test" "$INPUT_FILE" > test_output.txt
# Since we are redirecting to a file, isatty(STDOUT_FILENO) should be false
if cat -v test_output.txt | grep -q "\^\[\["; then
    echo -e "${RED}FAIL (Color should be disabled by default when piping to file)${NC}"
    exit 1
else
    echo -e "${GREEN}PASS${NC}"
fi

# Test 17: Case-Insensitive Match (-i)
echo -n "Test $((test_no++)): Case-Insensitive Match (-i)... "
# Search for uppercase 'HELLO' in the input file which contains 'hello'
if ./build/perg -i --no-color "HELLO" "$INPUT_FILE" | grep -q "hello world"; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL (Case-insensitive match failed)${NC}"
    exit 1
fi

echo -e "\n${CYAN}---------------------------------------${NC}"
echo -e "${GREEN}All $((test_no-1)) tests passed successfully!${NC}"