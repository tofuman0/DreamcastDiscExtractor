#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_dir=${1:-"$source_dir/build/linux"}
configuration=${2:-Release}

for tool in cmake ctest git; do
    if ! command -v "$tool" >/dev/null; then
        printf 'Install %s and add it to PATH.\n' "$tool" >&2
        exit 1
    fi
done

cmake -S "$source_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE="$configuration" -DBUILD_TESTING=ON
cmake --build "$build_dir" --config "$configuration" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
ctest --test-dir "$build_dir" -C "$configuration" --output-on-failure
