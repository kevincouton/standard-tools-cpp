# syntax=docker/dockerfile:1

# Classic multi-stage container image for standard-tools-cpp.
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    pkg-config \
    libssl-dev \
    libcurl4-openssl-dev \
    libpq-dev \
    libpqxx-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt .
COPY proto ./proto
COPY src ./src
COPY tests ./tests

RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTANDARD_TOOLS_ENABLE_GRPC=ON \
    -DSTANDARD_TOOLS_ENABLE_POSTGRES=ON \
    -DSTANDARD_TOOLS_ENABLE_CURL=ON \
    -DSTANDARD_TOOLS_BUILD_INTEGRATION_TESTS=OFF \
    -DSTANDARD_TOOLS_BUILD_E2E_TESTS=OFF \
    && cmake --build build --parallel $(nproc)

RUN mkdir -p /empty/tmp /empty/app && \
    useradd -u 65532 -r -s /usr/sbin/nologin nonroot || true && \
    chown -R 65532:65532 /empty

FROM gcr.io/distroless/cc-debian12:nonroot

COPY --from=builder --chown=nonroot:nonroot /src/build/server /usr/local/bin/server
COPY --from=builder --chown=nonroot:nonroot /src/build/cli /usr/local/bin/cli
COPY --from=builder --chown=nonroot:nonroot /src/build/healthcheck /usr/local/bin/healthcheck
COPY --from=builder --chown=nonroot:nonroot /src/build/grpc_health_check /usr/local/bin/grpc_health_check
COPY --from=builder --chown=nonroot:nonroot /empty/tmp /tmp
COPY --from=builder --chown=nonroot:nonroot /empty/app /app

USER nonroot:nonroot
WORKDIR /app
ENV HOME=/app

EXPOSE 8080 50051

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD ["/usr/local/bin/healthcheck", "http://127.0.0.1:8080/health", "127.0.0.1:50051"]

ENTRYPOINT ["/usr/local/bin/server"]
