#!/usr/bin/env python3

import argparse
import http.client
import json
import re
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import time
from pathlib import Path

DEFAULT_THREADS = (1, 8, 32)
DEFAULT_PUBLICATIONS = (1, 10, 100, 1000)
DEFAULT_DELIVERY_THREADS = (1, 4, 8)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Замер отклика DGDS: поднимает стенд, наполняет его, снимает запросы, "
                    "процессор и память сервера.")
    parser.add_argument("--scenario", choices=("lists", "delivery", "all"), default="lists")
    parser.add_argument("--binaries", default=".build/bin", help="каталог собранных программ")
    parser.add_argument("--root", help="каталог стенда (по умолчанию временный)")
    parser.add_argument("--content", type=int, default=1088890, help="размер публикуемого контента, байт")
    parser.add_argument("--publications", default=",".join(str(value) for value in DEFAULT_PUBLICATIONS),
                        help="число публикаций в каталоге через запятую")
    parser.add_argument("--threads", default=",".join(str(value) for value in DEFAULT_THREADS),
                        help="число потоков нагрузки через запятую")
    parser.add_argument("--delivery-threads", default=",".join(str(value) for value in DEFAULT_DELIVERY_THREADS),
                        help="число потоков нагрузки на выдаче через запятую")
    parser.add_argument("--seconds", type=int, default=5, help="длительность одного замера, секунд")
    parser.add_argument("--author", default="автор", help="имя автора публикации")
    parser.add_argument("--keep", action="store_true", help="не удалять каталог стенда после замера")

    return parser.parse_args()


def numbers(value):
    return [int(item) for item in value.split(",") if item]


def cpu_seconds(pid):
    text = subprocess.run(["ps", "-o", "time=", "-p", str(pid)], capture_output=True, text=True).stdout.strip()
    days, _, rest = text.rpartition("-")
    parts = [float(part) for part in rest.split(":")]
    while len(parts) < 3:
        parts.insert(0, 0.0)
    hours, minutes, seconds = parts

    return (float(days) if days else 0.0) * 86400 + hours * 3600 + minutes * 60 + seconds


def rss_kib(pid):
    text = subprocess.run(["ps", "-o", "rss=", "-p", str(pid)], capture_output=True, text=True).stdout.strip()

    return int(text)


def free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))

        return probe.getsockname()[1]


def api(certificate, port, path, body):
    context = ssl.create_default_context(cafile=str(certificate))
    connection = http.client.HTTPSConnection("127.0.0.1", port, context=context, timeout=30)
    connection.request("POST", path, json.dumps(body), {"Content-Type": "application/json"})
    response = connection.getresponse()
    payload = response.read().decode()
    connection.close()

    return json.loads(payload)


def start_server(binary, root):
    port = free_port()
    log = (root / "server.log").open("w")
    process = subprocess.Popen([str(binary), "--root", str(root), "--port", str(port), "--rate-limit", "0"],
                               stdout=log, stderr=subprocess.STDOUT)

    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.5):
                return process, port
        except OSError:
            time.sleep(0.1)

    process.terminate()
    raise SystemExit("сервер не поднялся, смотрите server.log")


def write_content(path, size):
    line = "строка демонстрационного текста для измерения нагрузки\n"
    text = ""
    while len(text.encode()) < size:
        text += line

    path.write_bytes(text.encode()[:size])

    return path


def device_key(directory):
    private = directory / "load.key"
    subprocess.run(["openssl", "genpkey", "-algorithm", "X25519", "-out", str(private)],
                   check=True, capture_output=True)
    der = subprocess.run(["openssl", "pkey", "-in", str(private), "-pubout", "-outform", "DER"],
                         check=True, capture_output=True).stdout

    return private, der[-32:].hex()


def find_value(node, key):
    if isinstance(node, dict):
        for name, value in node.items():
            if name == key and isinstance(value, str):
                return value
            found = find_value(value, key)
            if found:
                return found
    if isinstance(node, list):
        for item in node:
            found = find_value(item, key)
            if found:
                return found

    return None


def seed_example(binary, certificate, port, root, content):
    result = subprocess.run([str(binary), "--certificate", str(certificate), "--port", str(port),
                             "--device", str(root / "device"), "--text", str(content)],
                            capture_output=True, text=True)

    if result.returncode != 0:
        raise SystemExit("пример не наполнил стенд: " + result.stdout + result.stderr)

    return result.stdout


def synthesize(records, total):
    existing = sorted(records.glob("*.publication"))
    if not existing:
        raise SystemExit("в стенде нет ни одной записи публикации")

    raw = existing[0].read_bytes()
    present = len(existing)
    for index in range(present, total):
        identifier = 9000000000000000000 + index
        (records / f"{identifier}.publication").write_bytes(identifier.to_bytes(8, "big") + raw[8:])

    return len(list(records.iterdir()))


def sweep(binary, certificate, port, pid, name, path, body, threads, seconds):
    before_cpu, before_rss = cpu_seconds(pid), rss_kib(pid)
    result = subprocess.run([str(binary), "--certificate", str(certificate), "--port", str(port),
                             "--path", path, "--threads", str(threads), "--seconds", str(seconds),
                             "--body", body],
                            capture_output=True, text=True)
    after_cpu, after_rss = cpu_seconds(pid), rss_kib(pid)

    line = result.stdout.strip()
    match = re.search(r"RPS (\d+)", line)
    if not match:
        raise SystemExit("замер не удался: " + line + result.stderr.strip())

    rps = int(match.group(1))
    cores = (after_cpu - before_cpu) / max(seconds, 1)
    per_request = (after_cpu - before_cpu) / max(rps * seconds, 1) * 1e6

    print(f"{name}: потоков {threads}, {line}")
    print(f"    процессор сервера {after_cpu - before_cpu:+.2f} с ({cores:.2f} ядра), "
          f"{per_request:.0f} мкс на запрос, память {after_rss / 1024:.1f} МиБ")

    return rps


def lists_scenario(args, binaries, root, certificate, port, pid, credentials, author_credentials, publication):
    records = root / "metadata" / "publications"
    for total in numbers(args.publications):
        count = synthesize(records, total)
        for threads in numbers(args.threads):
            sweep(binaries / "dgds-loadgen", certificate, port, pid, f"каталог, публикаций {count}",
                  "/catalog", '{"version":1,"offset":"0","limit":"20"}', threads, args.seconds)

    for threads in numbers(args.threads):
        sweep(binaries / "dgds-loadgen", certificate, port, pid, "список покупок",
              "/purchases", json.dumps({"version": 1, "credentials": credentials["data"],
                                        "offset": "0", "limit": "20"}), threads, args.seconds)
        sweep(binaries / "dgds-loadgen", certificate, port, pid, "список публикаций автора",
              "/author-publications", json.dumps({"version": 1, "credentials": author_credentials["data"],
                                                  "offset": "0", "limit": "20"}), threads, args.seconds)


def delivery_scenario(args, binaries, root, certificate, port, pid, credentials, device):
    body = json.dumps({"version": 1, "credentials": credentials["data"],
                       "purchase_id": credentials["purchase"], "device_key": device})
    for threads in numbers(args.delivery_threads):
        sweep(binaries / "dgds-loadgen", certificate, port, pid, "выдача пакета", "/fetch-package",
              body, threads, args.seconds)


def main():
    args = parse_args()
    binaries = Path(args.binaries).resolve()
    for name in ("dgds", "dgds-example", "dgds-loadgen"):
        if not (binaries / name).exists():
            raise SystemExit(f"нет собранной программы {name} в {binaries}")

    root = Path(args.root) if args.root else Path(tempfile.mkdtemp(prefix="dgds-perf-"))
    root.mkdir(parents=True, exist_ok=True)
    process = None

    try:
        content = write_content(root / "content.txt", args.content)
        process, port = start_server(binaries / "dgds", root)
        certificate = root / "tls" / "server.crt"
        print(seed_example(binaries / "dgds-example", certificate, port, root, content).strip().splitlines()[0])
        print(f"стенд {root}, порт {port}")

        api(certificate, port, "/register", {"version": 1, "name": "нагрузка"})
        credentials = api(certificate, port, "/login", {"version": 1, "name": "нагрузка"})
        author_credentials = api(certificate, port, "/login", {"version": 1, "name": args.author})
        catalog = api(certificate, port, "/catalog", {"version": 1, "limit": "1"})
        publication = find_value(catalog, "publication_id")
        _, device = device_key(root)
        api(certificate, port, "/buy", {"version": 1, "credentials": credentials["data"],
                                        "publication_id": publication, "device_key": device})
        purchases = api(certificate, port, "/purchases", {"version": 1, "credentials": credentials["data"]})
        credentials["purchase"] = find_value(purchases, "purchase_id")

        if args.scenario in ("lists", "all"):
            lists_scenario(args, binaries, root, certificate, port, process.pid, credentials,
                           author_credentials, publication)
        if args.scenario in ("delivery", "all"):
            delivery_scenario(args, binaries, root, certificate, port, process.pid, credentials, device)
    finally:
        if process is not None:
            process.terminate()
            process.wait(timeout=10)
        if not args.keep:
            shutil.rmtree(root, ignore_errors=True)
        else:
            print(f"каталог стенда сохранён: {root}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
