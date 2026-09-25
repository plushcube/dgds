#!/bin/bash

set -eu

cd "$(dirname "$0")"

failures=0

check_absent() {
    local description="$1"
    local pattern="$2"
    shift 2

    local found
    found=$(grep -rIl --include='*.h' --include='*.cpp' -e "$pattern" "$@" 2>/dev/null || true)

    if [ -n "$found" ]; then
        echo "Нарушен слой: $description" >&2
        echo "$found" >&2
        failures=$((failures + 1))
    fi
}

check_absent "ядро не знает про HTTP, сервер и клиент" 'httplib\|dgds/server\|dgds/client' libs/core
check_absent "сценарии сервера не знают про поверхность и HTTP" 'httplib\|server/api\|server/http' \
    libs/server/src/services libs/server/include/dgds/server/services
check_absent "обработчики поверхности не знают про сценарии" 'services/' \
    libs/server/src/http libs/server/include/dgds/server/http
check_absent "клиентская библиотека не зависит от сервера" 'dgds/server' libs/client
check_absent "тесты ядра не знают ни про HTTP, ни про сервер" 'httplib\|dgds/server' tests/core

if [ "$failures" -ne 0 ]; then
    echo "❌ Проверка слоёв не пройдена." >&2
    exit 1
fi

echo "Слои не нарушены"
