FROM ubuntu:24.04 AS builder

# Пустая локаль ломает распаковку архивов, которые тянет CMake
ENV LANG=C.UTF-8 LC_ALL=C.UTF-8

RUN apt-get update \
    && apt-get install --yes --no-install-recommends ca-certificates cmake g++ libssl-dev make \
    && rm -rf /var/lib/apt/lists/*

ARG PATCH_VERSION=1

WORKDIR /source
COPY . .

RUN cmake -S . -B build -DWITH_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DPATCH_VERSION="${PATCH_VERSION}" \
    && cmake --build build --parallel

FROM ubuntu:24.04 AS runtime

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8

RUN apt-get update \
    && apt-get install --yes --no-install-recommends ca-certificates curl libssl3 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --system --gid 10001 dgds \
    && useradd --system --uid 10001 --gid dgds --home-dir /data --shell /usr/sbin/nologin dgds \
    && mkdir /data \
    && chown dgds:dgds /data

COPY --from=builder /source/build/bin/dgds /usr/local/bin/dgds

USER dgds
WORKDIR /data
VOLUME ["/data"]
EXPOSE 8443

HEALTHCHECK --interval=10s --timeout=5s --start-period=5s --retries=3 \
    CMD curl --fail --silent --show-error --cacert /data/tls/server.crt --request POST \
    --header "Content-Type: application/json" --data '{"version":1}' \
    https://127.0.0.1:8443/catalog || exit 1

ENTRYPOINT ["/usr/local/bin/dgds"]
CMD ["--root", "/data", "--host", "0.0.0.0", "--port", "8443"]
