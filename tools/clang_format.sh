#!/usr/bin/env bash
set -euo pipefail

mode="${1:-check}"
formatter="${CLANG_FORMAT:-clang-format}"
if [[ "$mode" != check && "$mode" != format ]]; then
    echo "Usage: $0 [check|format]" >&2
    exit 2
fi

"$formatter" --version
# Only maintained, tracked sources; exclude vendored code and build artifacts.
git ls-files -z -- '*.cpp' '*.h' ':!:third_party/**' |
while IFS= read -r -d '' file; do
    if [[ "$mode" == format ]]; then
        "$formatter" -i "$file"
    else
        diff -u "$file" <("$formatter" "$file") || {
            echo "::error file=$file::needs reformatting"
            exit 1
        }
    fi
done
