#!/bin/bash
# Verifies that a set of object files (or archives of object files) doesn't
# define any symbol twice. Used to check for ODR violation between files
# compiled with different compiler options.
#
# Usage: test_exports [object_files...]

if [ $# -eq 0 ]; then
    echo "${0##*/}: list of object files expected"
fi

for f in "$@"; do
    nm -g -P "$f" | awk '$2 ~ /[uvwA-Z]/ && $3 != "" { print $1 }' || exit 1
done | sort | uniq -d | grep '^'

if [ $? -eq 0 ]; then
    echo The above-mentioned symbols are duplicates
    echo FAIL
    exit 1
fi

echo PASS
