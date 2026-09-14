#!/bin/bash

set -u

cd "$(dirname "$0")/../.." || exit 1

image="plantuml/plantuml:1.2026.8"
architecture="docs/architecture"
renders=$(mktemp -d)
trap 'rm -rf "$renders"' EXIT

sources=("$architecture"/*.puml)

if command -v docker >/dev/null; then
    render() { docker run --rm -v "$PWD:$PWD" -v "$renders:$renders" -w "$PWD" "$image" "$@"; }
elif command -v plantuml >/dev/null; then
    echo "  Закреплённый рендерер недоступен, используется локальный plantuml: результат может отличаться по версии"
    render() { plantuml "$@"; }
else
    echo "  PlantUML недоступен, проверка рендеров пропущена"
    exit 0
fi

if ! render -tsvg -o "$renders" "${sources[@]}"; then
    echo "❌ Диаграммы не собираются"
    exit 1
fi

status=0

for source in "${sources[@]}"; do
    name=$(basename "$source" .puml)
    committed="$architecture/$name.svg"

    if [ ! -f "$committed" ]; then
        echo "❌ Нет рендера для $source"
        status=1
    elif ! cmp -s "$committed" "$renders/$name.svg"; then
        echo "❌ Рендер устарел: $committed"
        status=1
    fi
done

for committed in "$architecture"/*.svg; do
    if [ ! -f "${committed%.svg}.puml" ]; then
        echo "❌ Рендер без исходника: $committed"
        status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "  Рендеры соответствуют исходникам"
fi

exit "$status"
