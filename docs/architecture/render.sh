#!/bin/bash

set -eu

cd "$(dirname "$0")/../.." || exit 1

image="plantuml/plantuml:1.2026.8"
architecture="docs/architecture"
manifest="$architecture/renders.sha256"

if command -v docker >/dev/null; then
    render() { docker run --rm -v "$PWD:$PWD" -w "$PWD" "$image" "$@"; }
elif command -v plantuml >/dev/null; then
    echo "Закреплённый рендерер недоступен, используется локальный plantuml"
    render() { plantuml "$@"; }
else
    echo "PlantUML недоступен: рендер невозможен" >&2
    exit 1
fi

render -tsvg "$architecture"/*.puml

: > "$manifest"

for source in "$architecture"/*.puml; do
    name=$(basename "$source" .puml)
    printf '%s %s %s\n' "$name" "$(git hash-object "$source")" \
        "$(git hash-object "$architecture/$name.svg")" >> "$manifest"
done

echo "Рендеры обновлены: $manifest"
