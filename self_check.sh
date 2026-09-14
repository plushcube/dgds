#!/bin/bash

echo "Running self-check..."

echo "  Checking format..."
if ! command -v clang-format >/dev/null; then
    echo "❌ clang-format not found."
    exit 1
fi

sources=$(find libs stubs src example tests -type f \( -name '*.h' -o -name '*.cpp' \) 2>/dev/null)
if [ -n "$sources" ] && ! echo "$sources" | xargs clang-format --dry-run --Werror; then
    echo "❌ Formatting check failed."
    exit 1
fi

echo "  Checking architecture renders..."
if ! ./docs/architecture/check_renders.sh; then
    exit 1
fi

echo "  Building..."
cmake -S . -B .build >/dev/null
cmake --build .build --parallel >/dev/null

echo "  Testing..."
if ! ctest --test-dir .build --output-on-failure >/dev/null; then
    echo "❌ Tests failed."
    exit 1
fi

echo "✅ Self-check completed successfully."
