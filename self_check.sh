#!/bin/bash

echo "Running self-check..."

echo "  Checking format..."
if ! command -v clang-format >/dev/null; then
    echo "❌ clang-format not found."
    exit 1
fi

sources=$(find libs src tests tools -type f \( -name '*.h' -o -name '*.cpp' \) 2>/dev/null)
if [ -n "$sources" ] && ! echo "$sources" | xargs clang-format --dry-run --Werror; then
    echo "❌ Formatting check failed."
    exit 1
fi

echo "  Checking cmake format..."
if ! command -v cmake-format >/dev/null; then
    echo "❌ cmake-format not found."
    exit 1
fi

cmake_files=$(find . -name CMakeLists.txt -o -name '*.cmake' | grep -v '^./.build' | grep -v '^./.git/' | sort)
if [ -n "$cmake_files" ] && ! echo "$cmake_files" | xargs cmake-format --check; then
    echo "❌ CMake formatting check failed."
    exit 1
fi

echo "  Checking architecture renders..."
if ! ./docs/architecture/check_renders.sh; then
    exit 1
fi

echo "  Checking layers..."
if ! ./check_layers.sh; then
    exit 1
fi

echo "  Building..."
if ! cmake -S . -B .build >/dev/null; then
    echo "❌ Configure failed."
    exit 1
fi

if ! cmake --build .build --parallel >/dev/null; then
    echo "❌ Build failed."
    exit 1
fi

echo "  Testing..."
test_log=$(mktemp)
trap 'rm -f "$test_log"' EXIT

if ! ctest --test-dir .build --output-on-failure >"$test_log" 2>&1; then
    echo "❌ Tests failed."
    cat "$test_log"
    exit 1
fi

echo "✅ Self-check completed successfully."
