#!/bin/bash

set -u

cd "$(dirname "$0")/../.." || exit 1

architecture="docs/architecture"
manifest="$architecture/renders.sha256"
status=0

if [ ! -f "$manifest" ]; then
    echo "❌ Нет манифеста рендеров: $manifest"
    exit 1
fi

while read -r name puml_hash render_hash; do
    source="$architecture/$name.puml"
    render_file="$architecture/$name.svg"

    if [ ! -f "$source" ]; then
        echo "❌ Исходник из манифеста пропал: $source"
        status=1
        continue
    fi

    if [ ! -f "$render_file" ]; then
        echo "❌ Нет рендера для $source"
        status=1
        continue
    fi

    if [ "$(git hash-object "$source")" != "$puml_hash" ]; then
        echo "❌ Рендер устарел: $render_file"
        status=1
    elif [ "$(git hash-object "$render_file")" != "$render_hash" ]; then
        echo "❌ Рендер изменён в обход render.sh: $render_file"
        status=1
    fi
done < "$manifest"

for source in "$architecture"/*.puml; do
    name=$(basename "$source" .puml)

    if ! grep -q "^$name " "$manifest"; then
        echo "❌ Нет записи в манифесте: $source"
        status=1
    fi
done

for render_file in "$architecture"/*.svg; do
    if [ ! -f "${render_file%.svg}.puml" ]; then
        echo "❌ Рендер без исходника: $render_file"
        status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "  Рендеры соответствуют исходникам"
fi

exit "$status"
