# standard-tools-cpp

A C++ quantitative-finance toolkit that mirrors the production-ready foundation
of `standard-tools-go`. It provides REST and gRPC endpoints, hash-chained audit
trails, agent tool dispatch, and PostgreSQL persistence.

## Features

- **Core domain models**: `Ticker`, `DateRange`, `BarInterval`, `OHLCV`, `TickerInfo`, `FinancialRatios`, `DataSetMetadata`.
- **Market data**: provider interface, in-memory cache, synthetic provider, and service.
- **Audit**: hash-chained `DecisionRecord`, in-memory and PostgreSQL storage, verifier, and replay.
- **Agent**: tool registry and dispatcher with `health`, `list_tools`, and `fetch_ohlcv` tools.
- **API**: Crow-based REST routes, A2A and MCP skeleton endpoints, gRPC health service.
- **Storage**: libpqxx PostgreSQL pool with embedded schema migrations.
- **CLI**: `verify`, `report`, `replay`, `keygen`, `anchor` commands.
- **Tests**: Catch2 unit tests, integration tests gated by a CMake flag, and E2E tests that spawn the server.
- **Containers**: classic `Dockerfile` and `Dockerfile.native` (static/scratch attempt).
- **Local CI**: `act` + `podman` via `scripts/run-act-local.sh`.
- **Task runner**: `mise.toml` tasks for configure, build, test, image, and smoke.

## Dependencies

The CMake build fetches header-only dependencies automatically
(nlohmann/json, Catch2, CLI11, toml++, Crow, Howard Hinnant's date).

System dependencies required for a full build:

- CMake >= 3.25
- C++20 compiler
- OpenSSL
- libcurl
- gRPC / protobuf
- libpqxx

On Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libssl-dev \
  libcurl4-openssl-dev libpq-dev libpqxx-dev libgrpc++-dev libprotobuf-dev \
  protobuf-compiler protobuf-compiler-grpc
```

On macOS (with Homebrew):

```bash
brew install cmake openssl curl libpq libpqxx grpc protobuf
```

If a system dependency is missing, the corresponding feature is automatically
disabled (`STANDARD_TOOLS_ENABLE_GRPC`, `STANDARD_TOOLS_ENABLE_POSTGRES`,
`STANDARD_TOOLS_ENABLE_CURL`).

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
```

To also build integration and E2E tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DSTANDARD_TOOLS_BUILD_INTEGRATION_TESTS=ON \
  -DSTANDARD_TOOLS_BUILD_E2E_TESTS=ON
cmake --build build --parallel $(nproc)
```

Or with mise:

```bash
mise run configure
mise run build
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

Or with mise:

```bash
mise run test
mise run test-integration   # needs SQT_DATABASE_URL
mise run test-e2e
```

## Run

```bash
./build/server
./build/cli health
```

Configuration is loaded from optional TOML files and environment variables with
the `SQT_` prefix. Nested keys use double underscores, e.g.
`SQT_POLYGON__API_KEY`.

## API

- `GET /health` — health check
- `GET /api/v1/agent/tools` — list agent tools
- `POST /api/v1/agent/dispatch` — dispatch a tool
- `GET /api/v1/market-data/<ticker>` — fetch OHLCV
- `GET /a2a/agent.json` — A2A agent card
- `POST /a2a/tasks` — A2A task dispatch
- `GET /mcp/capabilities` — MCP capabilities
- `POST /mcp/tools/list` — MCP list tools
- `POST /mcp/tools/call` — MCP call tool
- gRPC `grpc.health.v1.Health` on port `SQT_GRPC_PORT`

## Containers

```bash
mise run image
mise run smoke
mise run image-native
mise run smoke-native
```

## Local CI

```bash
mise run act
```

This runs the `quality` job in `act`, executes integration tests against a local
PostgreSQL container, and smoke-tests both container images with podman.

## License

MIT
