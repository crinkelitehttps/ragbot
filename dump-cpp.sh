#!/bin/bash

# Script: find-cpp-files.sh
# Description: Finds and displays all .h, .hpp, and .cpp files with their contents

# Color codes for better terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Function to display file with content
display_file() {
    local file="$1"
    local size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null)
    
    # Optional: skip very large files (uncomment to enable)
    # if [ "$size" -gt 1000000 ]; then
    #     echo -e "${RED}Skipping large file: $file (${size} bytes)${NC}"
    #     return
    # fi
    
    echo -e "${GREEN}=== $(basename "$file") ===${NC}"
    echo
    cat "$file"
    echo
}

# Find and process all matching files
echo -e "${YELLOW}Finding C/C++ files...${NC}"
echo "=============================================="

find . -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" \) -print0 | \
while IFS= read -r -d '' file; do
    display_file "$file"
done

echo "=============================================="
echo -e "${GREEN}Done!${NC}"

