# Port Parity Matrix

This document compares the `standard-tools-cpp` port against the other Standard-Tools language implementations. It is accurate as of the latest commit on `main`.

## Legend

- ✅ Implemented / available
- ⚠️ Partial, stub, or minimal implementation
- ❌ Not implemented
- N/A Not applicable for this transport/stack

## Transport & protocol support

| Feature | C++ | C# | Kotlin | Go | Rust |
|---|---|---|---|---|---|
| REST | ✅ | ✅ | ✅ | ✅ | ✅ |
| gRPC | ⚠️ health only | ❌ | ✅ | ⚠️ health only | ⚠️ health + agent |
| A2A | ⚠️ skeleton | ❌ | ⚠️ tasks/send, no streaming | ⚠️ minimal | ⚠️ partial (get/cancel placeholders) |
| MCP | ⚠️ HTTP-only | ❌ | ✅ SSE | ⚠️ HTTP-only | ⚠️ HTTP-only |
| SSE | ❌ | ❌ | ⚠️ MCP transport only | ❌ | ❌ |
| Docker / container image | ✅ | ❌ | ✅ | ✅ | ✅ |
| CLI | ✅ | ❌ | ⚠️ audit commands only | ✅ | ⚠️ server + audit placeholders |
| Container health checks | ✅ | ⚠️ HTTP only | ⚠️ actuator only | ✅ | ❌ |

## Domain modules

| Feature | C++ | C# | Kotlin | Go | Rust |
|---|---|---|---|---|---|
| Market data provider port | ⚠️ synthetic only | ⚠️ interface / stub | ✅ YF, Polygon, Bloomberg stub | ✅ synthetic, YF, Polygon | ✅ YF + Moka cache |
| Indicators | ✅ | ✅ | ✅ | ✅ | ✅ |
| Risk / return metrics | ✅ | ✅ | ✅ | ✅ | ✅ |
| Analysis (regression, cointegration, Hurst, PCA, correlation, options) | ⚠️ no multi-factor | ✅ library; ⚠️ only regression + options exposed | ✅ | ⚠️ no multi-factor | ⚠️ no multi-factor |
| Backtesting engine | ✅ | ✅ | ✅ | ✅ | ✅ |
| Walk-forward optimization | ✅ | ✅ | ✅ | ✅ | ✅ |
| Monte Carlo simulation | ✅ | ✅ | ✅ | ✅ | ✅ |
| Robustness / stress testing | ❌ | ❌ | ✅ | ❌ | ✅ |
| Portfolio mean-variance | ✅ | ✅ | ✅ | ✅ | ✅ |
| Portfolio risk parity | ✅ equal-risk-contribution | ⚠️ inverse-vol | ✅ equal-risk-contribution | ✅ equal-risk-contribution | ✅ equal-risk-contribution |
| Black-Litterman | ✅ | ✅ | ✅ | ✅ | ✅ |
| Screener | ⚠️ hardcoded provider | ⚠️ hardcoded provider | ⚠️ hardcoded provider | ⚠️ hardcoded provider | ⚠️ hardcoded provider |
| Hash-chained audit | ✅ | ✅ | ✅ | ✅ | ✅ |
| Agent tool dispatcher | ✅ (11 tools) | ✅ | ✅ | ✅ (19 tools) | ✅ (42 tools) |

## Security & audit

| Feature | C++ | C# | Kotlin | Go | Rust |
|---|---|---|---|---|---|
| API-key auth on REST | ✅ fail-closed | ✅ fail-closed | ✅ fail-closed | ✅ fail-closed | ✅ fail-closed |
| API-key auth on gRPC | ❌ | N/A | ✅ | ✅ | ✅ |
| TLS termination | ❌ | ❌ | ❌ | ❌ | ❌ |
| Audit provenance (git commit / version / seed) | ✅ all three | ⚠️ schema only | ⚠️ commit + version | ✅ all three | ❌ none recorded |
| Replay read-only / side-effect blocklist | ⚠️ read-only fetch, no re-execution | ❌ not implemented | ✅ blocklist | ❌ re-executes | ⚠️ blocklist, CLI placeholder |
| Persistent audit storage | ✅ PostgreSQL + memory | ❌ in-memory only | ✅ PostgreSQL | ✅ PostgreSQL + memory | ✅ PostgreSQL + memory |

## Operational hardening

| Feature | C++ | C# | Kotlin | Go | Rust |
|---|---|---|---|---|---|
| Request body limit | 16 MiB | 16 MiB | 16 MB + 4 MB gRPC | 16 MiB | 16 MiB |
| HTTP/gRPC request timeout | ❌ | configured | 30 s netty | configured | 60 s |
| Backtest bar cap | 50 000 | 50 000 | 50 000 | 50 000 | 50 000 |
| Monte Carlo simulation cap | 100 000 | 100 000 | 100 000 / 2 520 horizon | 100 000 | 10 000 |
| Walk-forward window cap | 10 000 | 10 000 | 10 000 | 10 000 | 10 000 |
| Walk-forward combination cap | 10 000 | 10 000 | 10 000 | 10 000 | 10 000 |
| Portfolio asset cap | 100 | 100 | 100 | 100 | 100 |
| Screener ticker cap | 500 | 500 | 500 | 500 | 100 |
| Structured logging / request tracing | ❌ | ❌ | ❌ | ❌ | ❌ |
| Metrics / Prometheus endpoint | ❌ | ❌ | ✅ | ❌ | ❌ |

## CI status

Validation below was performed locally with `nektos/act` on `linux/arm64` (Podman) using the workflow job(s) that exercise the core build and tests.

| Port | Status | Notes |
|---|---|---|
| C++ | ⚠️ pending | `quality` job is running; old `rm -rf /var/lib/apt/lists/*` issue already removed |
| C# | ✅ green | `act push --job build-and-test` passes |
| Kotlin | ✅ green | `act push --job unit-tests` passes; native build not validated locally |
| Go | ✅ green | `act push --job quality` passes |
| Rust | ⚠️ pending | `quality` job passes; `test` job fixed to skip artifact upload under `env.ACT` and is re-running |

## Known limitations relevant to this port

- gRPC exposes only the standard `grpc.health.v1` health service and has no API-key interceptor.
- A2A and MCP are skeleton HTTP endpoints, not full protocol implementations.
- No live market-data provider; only synthetic data is available.
- No HTTP/gRPC request timeouts or structured logging.
- CI fails in the dependency-install step due to missing permissions.

## Outstanding P0/P1 gaps (deferred)

The following items were identified in the staff-engine audit and are explicitly documented rather than hidden behind false claims:

1. **TLS termination** — not implemented in any port. Deploy behind a reverse proxy that terminates TLS.
2. **Structured logging / request tracing** — no request-id propagation or structured log output.
3. **Full A2A/MCP semantics** — A2A and MCP are skeleton HTTP endpoints, not full protocol implementations.
4. **gRPC auth interceptor** — gRPC exposes only `grpc.health.v1.Health` and has no API-key interceptor.
5. **HTTP/gRPC request timeouts** — not configured; long-running requests can hang indefinitely.
6. **Live market-data adapter** — only synthetic market data is available; the `SQT_POLYGON__API_KEY` config key is not consumed by a live provider.
7. **Dependency scanning** — add Dependabot or a C++ equivalent (e.g., OWASP dependency-check) to CI.

## Recommendations before a release tag

1. Add a gRPC auth interceptor and request timeouts.
2. Implement a live market-data adapter or remove the `SQT_POLYGON__API_KEY` config key.
3. Add dependency scanning to CI.
