#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

# Output colors
GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Trackers
total_tests=0
passed_tests=0
test_num=1

# Helper to auto-increment test header
it() {
    local label="$1"
    echo -n "Test $((test_num++)): $label... "
}

# Helper to evaluate results and dump output on failure
check_result() {
    local status=$1
    local reason=$2
    ((total_tests++))
    if [ $status -eq 0 ]; then
        echo -e "${GREEN}PASS${NC}"
        ((passed_tests++))
    else
        echo -e "${RED}FAIL${NC}"
        echo -e "   ${YELLOW}[!] Reason:${NC} $reason"
        echo -e "   ${CYAN}[!] Actual Output DUMP ($-marked line endings):${NC}"
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
make -j$(nproc) > /dev/null
cd "$PROJECT_ROOT"

# Input setup
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

it "Simple Exact Match"
./build/perg --no-color "hello" "$INPUT_FILE" > test_output.txt
grep -q "hello world" test_output.txt
check_result $? "Exact match 'hello' not found"

it "Regex Match (finding 2026)"
./build/perg --no-color "[0-9]{4}" "$INPUT_FILE" > test_output.txt
grep -q "2026" test_output.txt
check_result $? "Regex [0-9]{4} failed"

it "Multiple Matches on one line"
./build/perg --no-color "test" "$INPUT_FILE" > test_output.txt
count=$(grep -o "test" test_output.txt | wc -l)
[ "$count" -eq 6 ]
check_result $? "Expected 6 matches, got $count"

it "Results Ordered by Ascending Line"
LARGE_FILE="large_input.txt"
for i in {1..100}; do
    echo "line $i: match_this" >> "$LARGE_FILE"
done
./build/perg --no-color "match_this" "$LARGE_FILE" | awk '{print $1}' > test_output.txt
sort -n -c test_output.txt 2>/dev/null
check_result $? "Lines are out of order (Parallel chunking sync failed)"
rm "$LARGE_FILE"

it "Missing File Error Handling"
if ! ./build/perg --no-color "test" non_existent_file.txt 2>/dev/null; then
    check_result 0 "Properly caught missing file"
else
    check_result 1 "Command should have failed for non_existent_file.txt"
fi

it "Help Function (-h)"
./build/perg -h | grep -iq "usage"
check_result $? "Help text missing or -h flag failed"

it "Count Flag (-c) Total Occurrences"
actual_count=$(./build/perg --no-color -c "test" "$INPUT_FILE" | tr -d '[:space:]')
[ "$actual_count" -eq 6 ]
check_result $? "Expected count 6, got '$actual_count'"

it "Complex Regex (hello|final)"
./build/perg --no-color "hello|final" "$INPUT_FILE" > test_output.txt
count=$(grep -cE "hello world|final line" test_output.txt)
[ "$count" -eq 2 ]
check_result $? "Complex regex | failed"

it "Literal Duplicate Line Check"
./build/perg --no-color "test" "$INPUT_FILE" > test_output.txt
line_count=$(grep -c "test" test_output.txt)
[ "$line_count" -eq 4 ]
check_result $? "Expected 4 unique lines, got $line_count"

it "Graph Duplicate Line Prevention"
./build/perg -g "test" "$INPUT_FILE" > test_output.txt
line_3_count=$(grep -c "multiple matches" test_output.txt)
[ "$line_3_count" -eq 1 ]
check_result $? "Graph duplicated lines for multiple matches on one line"

it "Context Formatting (-C 1)"
./build/perg -n --no-color -C 1 "this is a test line" "$INPUT_FILE" > test_output.txt
grep -qE "^1[[:space:]]+- hello world" test_output.txt && \
grep -qE "^2[[:space:]]+: this is a test line" test_output.txt && \
grep -qE "^3[[:space:]]+- multiple matches" test_output.txt
check_result $? "Context format mismatch"

it "Color Output Verification (--color)"
./build/perg --color "test" "$INPUT_FILE" > test_output.txt
cat -v test_output.txt | grep -q "\^\[\["
check_result $? "No ANSI codes found with --color"

it "Explicit No-Color Verification (--no-color)"
./build/perg --no-color "test" "$INPUT_FILE" > test_output.txt
! cat -v test_output.txt | grep -q "\^\[\["
check_result $? "ANSI codes found despite --no-color"

it "Case-Insensitive Match (-i)"
./build/perg -i --no-color "HELLO" "$INPUT_FILE" | grep -q "hello world"
check_result $? "Case-insensitive match failed"

it "Context Overlap Prevention"
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

it "Recursive Directory Search"
./build/perg --no-color -r "target" "$TEST_DIR" > test_output.txt
r_count=$(grep -c "target" test_output.txt)
[ "$r_count" -eq 3 ]
check_result $? "Expected 3 matches, got $r_count"

it "Filename Suffix Check"
./build/perg -n -rf --no-color "target" "$TEST_DIR" > test_output.txt
grep -qE "^1[[:space:]]+: target in root \| .*file1\.txt" test_output.txt && \
grep -qE "^1[[:space:]]+: target in sub1 \| .*sub1/file2\.txt" test_output.txt
check_result $? "Filename suffix pipe format failed"

it "Binary File Skipping Check"
printf "match\0binary" > "$TEST_DIR/binary_file.dat"
./build/perg --no-color -r "match" "$TEST_DIR" > test_output.txt
! grep -q "binary_file.dat" test_output.txt
check_result $? "Failed to skip binary file"

it "Directory without -r (Error Check)"
./build/perg --no-color "target" "$TEST_DIR" 2>test_output.txt
grep -q "Is a directory (use -r)" test_output.txt
check_result $? "Incorrect error message for directory"

it "Extension Filter (-e .txt)"
echo "target in log" > "$TEST_DIR/exclude_me.log"
./build/perg -rf -e ".txt" --no-color "target" "$TEST_DIR" > test_output.txt
grep -q "file1.txt" test_output.txt && ! grep -q "exclude_me.log" test_output.txt
check_result $? "Filter -e failed to exclude .log"

echo -e "\nRunning Graph Visualization Tests..."

it "Graph View Structure (-g)"
./build/perg -gr "target" "$TEST_DIR" > test_output.txt
grep -qE "[-|+]-- sub1/ \([0-9]+\)" test_output.txt && \
grep -qE "[-|+]-- subsub/ \([0-9]+\)" test_output.txt
check_result $? "Graph tree structure missing folders or counts"

it "Graph View Content Format"
./build/perg -gr "target" "$TEST_DIR" > test_output.txt
grep -qE "[-|+]-- 1[[:space:]]+: target" test_output.txt
check_result $? "Graph line format mismatch"

it "Graph View with Count Only"
./build/perg -gr -c "target" "$TEST_DIR" > test_output.txt
grep -q "file1.txt (1)" test_output.txt && ! grep -q "target in root" test_output.txt
check_result $? "Graph -c should hide match content"

echo -e "\n${CYAN}---------------------------------------${NC}"
if [ $passed_tests -eq $total_tests ]; then
    echo -e "${GREEN}SUCCESS: $passed_tests/$total_tests tests passed!${NC}"
else
    echo -e "${RED}FAILURE: Only $passed_tests/$total_tests tests passed.${NC}"
    exit 1
fi