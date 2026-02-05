#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

# output colors
GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Trackers
total_tests=0
passed_tests=0

# Helper function to evaluate results without exiting
check_result() {
    local status=$1
    local name=$2
    ((total_tests++))
    if [ $status -eq 0 ]; then
        echo -e "${GREEN}PASS${NC}"
        ((passed_tests++))
    else
        echo -e "${RED}FAIL${NC}"
        echo -e "  ${YELLOW}[!] Reason:${NC} $name"
        echo -e "  ${CYAN}[!] Actual Output DUMP:${NC}"
        echo "---------------------------------------"
        if [ -f test_output.txt ]; then
            cat -A test_output.txt | sed 's/^/      /'
        else
            echo "      (No output file found)"
        fi
        echo "---------------------------------------"
    fi
}

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

# input setup
INPUT_FILE="input.txt"
cat <<EOF > "$INPUT_FILE"
hello world
this is a test line with 2026 inside
multiple matches: test, test, and test
final line
another test line
adjacent test line
EOF

echo -e "\nRunning Integration Tests (Standard Mode)..."

# Test 1: Simple Exact Match
echo -n "Test 1: Simple Exact Match... "
./build/perg --no-color "hello" "$INPUT_FILE" > test_output.txt
grep -q "hello world" test_output.txt
check_result $? "Exact match 'hello' not found"

# Test 2: Regex Match
echo -n "Test 2: Regex Match (finding 2026)... "
./build/perg --no-color "[0-9]{4}" "$INPUT_FILE" > test_output.txt
grep -q "2026" test_output.txt
check_result $? "Regex [0-9]{4} failed"

# Test 3: Multiple Matches on one line
echo -n "Test 3: Multiple Matches on one line... "
./build/perg --no-color "test" "$INPUT_FILE" > test_output.txt
count=$(grep -o "test" test_output.txt | wc -l)
[ "$count" -eq 6 ]
check_result $? "Expected 6 matches, got $count"

# Test 4: Missing File Error
echo -n "Test 4: Missing File Error Handling... "
if ! ./build/perg --no-color "test" non_existent_file.txt 2>/dev/null; then
    check_result 0 "Properly caught missing file"
else
    check_result 1 "Command should have failed for non_existent_file.txt"
fi

# Test 5: Help Function (-h)
echo -n "Test 5: Help Function (-h)... "
./build/perg -h | grep -iq "usage"
check_result $? "Help text missing or -h flag failed"

# Test 6: Count Flag (-c) Total Occurrences
echo -n "Test 6: Count Flag (-c) Total Occurrences... "
actual_count=$(./build/perg --no-color -c "test" "$INPUT_FILE" | tr -d '[:space:]')
[ "$actual_count" -eq 6 ]
check_result $? "Expected count 6, got '$actual_count'"

# Test 7: Complex Regex (Alternative/Grouping)
echo -n "Test 7: Complex Regex (hello|final)... "
./build/perg --no-color "hello|final" "$INPUT_FILE" > test_output.txt
count=$(grep -cE "hello world|final line" test_output.txt)
[ "$count" -eq 2 ]
check_result $? "Complex regex | failed"

# Test 8: Literal Duplicate Line Check
echo -n "Test 8: Literal Duplicate Line Check... "
./build/perg --no-color "test" "$INPUT_FILE" > test_output.txt
line_count=$(grep -c "test" test_output.txt)
[ "$line_count" -eq 4 ]
check_result $? "Expected 4 unique lines, got $line_count"

# Test 23: Graph Duplicate Line Prevention
echo -n "Test 23: Graph Duplicate Line Prevention... "
# The input line "multiple matches: test, test, and test" has 3 matches but should appear ONCE
./build/perg -g "test" "$INPUT_FILE" > test_output.txt
# Count how many times line 3 appears in the graph
line_3_count=$(grep -c "multiple matches" test_output.txt)
if [ "$line_3_count" -eq 1 ]; then
    check_result 0 "Graph correctly grouped multiple matches on one line"
else
    check_result 1 "Graph duplicated lines (Expected 1, got $line_3_count)"
fi

# Test 9: Context Formatting
echo -n "Test 9: Context Formatting (-C 1)... "
./build/perg -n --no-color -C 1 "this is a test line" "$INPUT_FILE" > test_output.txt
grep -qE "^1[[:space:]]+- hello world" test_output.txt && \
grep -qE "^2[[:space:]]+: this is a test line" test_output.txt && \
grep -qE "^3[[:space:]]+- multiple matches" test_output.txt
check_result $? "Context format mismatch"

# Test 10: Color Output Verification
echo -n "Test 10: Color Output Verification (--color)... "
./build/perg --color "test" "$INPUT_FILE" > test_output.txt
cat -v test_output.txt | grep -q "\^\[\["
check_result $? "No ANSI codes found with --color"

# Test 11: Explicit No-Color Verification (--no-color)
echo -n "Test 11: Explicit No-Color Verification (--no-color)... "
./build/perg --no-color "test" "$INPUT_FILE" > test_output.txt
! cat -v test_output.txt | grep -q "\^\[\["
check_result $? "ANSI codes found despite --no-color"

# Test 12: Default Behavior
echo -n "Test 12: Default Color Behavior (Pipe Check)... "
./build/perg "test" "$INPUT_FILE" > test_output.txt
! cat -v test_output.txt | grep -q "\^\[\["
check_result $? "Color should be off when redirecting to file"

# Test 13: Case-Insensitive Match (-i)
echo -n "Test 13: Case-Insensitive Match (-i)... "
./build/perg -i --no-color "HELLO" "$INPUT_FILE" | grep -q "hello world"
check_result $? "Case-insensitive match failed"

# Test 14: Context Overlap Prevention
echo -n "Test 14: Context Overlap Prevention... "
./build/perg --no-color -C 1 "another test" "$INPUT_FILE" > test_output.txt
occur_count=$(grep -c "another test line" test_output.txt)
[ "$occur_count" -eq 1 ]
check_result $? "Detected duplicate lines in context overlap"

echo -e "\nRunning Directory & Recursive Tests..."

# Folder setup
TEST_DIR="temp_test_dir"
mkdir -p "$TEST_DIR/sub1/subsub"
mkdir -p "$TEST_DIR/sub2"
echo "target in root" > "$TEST_DIR/file1.txt"
echo "target in sub1" > "$TEST_DIR/sub1/file2.txt"
echo "target in subsub" > "$TEST_DIR/sub1/subsub/file3.txt"
echo "no match here" > "$TEST_DIR/sub2/file4.txt"

# Test 15: Recursive Directory Search
echo -n "Test 15: Recursive Directory Search... "
./build/perg --no-color -r "target" "$TEST_DIR" > test_output.txt
r_count=$(grep -c "target" test_output.txt)
[ "$r_count" -eq 3 ]
check_result $? "Expected 3 matches, got $r_count"

# Test 16: Filename Suffix Check
echo -n "Test 16: Filename Suffix Check (Pipe Format)... "
./build/perg -n -r -F --no-color "target" "$TEST_DIR" > test_output.txt
grep -qE "^1[[:space:]]+: target in root \| .*file1\.txt" test_output.txt && \
grep -qE "^1[[:space:]]+: target in sub1 \| .*sub1/file2\.txt" test_output.txt
check_result $? "Filename suffix pipe format failed"

# Test 17: Binary File Skipping
echo -n "Test 17: Binary File Skipping Check... "
printf "match\0binary" > "$TEST_DIR/binary_file.dat"
./build/perg --no-color -r "match" "$TEST_DIR" > test_output.txt
! grep -q "binary_file.dat" test_output.txt
check_result $? "Failed to skip binary file"

# Test 18: Directory without -r
echo -n "Test 18: Directory without -r (Error Check)... "
if ! ./build/perg --no-color "target" "$TEST_DIR" 2>test_output.txt; then
    grep -q "Is a directory (use -r to recurse)" test_output.txt
    check_result $? "Incorrect error message for directory"
else
    check_result 1 "Should have failed without -r"
fi

# Test 19: Extension Filter (-e)
echo -n "Test 19: Extension Filter (-e .txt)... "
echo "target in log" > "$TEST_DIR/exclude_me.log"
./build/perg -r -F -e ".txt" --no-color "target" "$TEST_DIR" > test_output.txt
grep -q "file1.txt" test_output.txt && ! grep -q "exclude_me.log" test_output.txt
check_result $? "Filter -e failed to exclude .log"

echo -e "\nRunning Graph Visualization Tests..."

# Test 20: Graph View Structure (-g)
echo -n "Test 20: Graph View Structure (-g)... "
./build/perg -gr "target" "$TEST_DIR" > test_output.txt

if grep -Fq "+-- sub1/" test_output.txt && grep -Fq "+-- subsub/" test_output.txt; then
    check_result 0 "Graph tree structure folders found"
else
    check_result 1 "Graph tree structure missing folders"
fi

# Test 21: Graph View Content Format
echo -n "Test 21: Graph View Content Format... "
# Match: Any branch prefix, then line number '1', then spaces, then ':', then 'target'
grep -qE "[^[:alnum:]]+1[[:space:]]+: target" test_output.txt
check_result $? "Graph line format mismatch"

# Test 22: Graph View with Count Only
echo -n "Test 22: Graph View Count Only (-g -c)... "
./build/perg -gr -c "target" "$TEST_DIR" > test_output.txt
# Should show file name and count but NOT the content lines
grep -q "file1.txt (1)" test_output.txt && ! grep -q "target in root" test_output.txt
check_result $? "Graph -c should hide match content"

echo -e "\n${CYAN}---------------------------------------${NC}"
if [ $passed_tests -eq $total_tests ]; then
    echo -e "${GREEN}SUCCESS: $passed_tests/$total_tests tests passed!${NC}"
else
    echo -e "${RED}FAILURE: Only $passed_tests/$total_tests tests passed.${NC}"
    exit 1
fi