#!/usr/bin/env bash
# Runs the CI workflow's quality job locally with nektos/act, runs the
# integration tests against a local PostgreSQL container, then builds and
# smoke-tests both container images directly with podman.
set -euo pipefail

cd "$(dirname "$0")/.."

command -v cmake >/dev/null 2>&1 || { echo "cmake is required" >&2; exit 1; }
command -v curl >/dev/null 2>&1 || { echo "curl is required for smoke tests" >&2; exit 1; }
[ -x ./scripts/visual-test-report.sh ] || { echo "Missing ./scripts/visual-test-report.sh" >&2; exit 1; }
[ -x ./scripts/smoke-test-image.sh ] || { echo "Missing ./scripts/smoke-test-image.sh" >&2; exit 1; }

read -ra ENGINE_CMD <<< "${ENGINE:-podman}"

DOCKER_HOST=""
if command -v podman >/dev/null 2>&1; then
  if podman machine ls >/dev/null 2>&1; then
    running=$(podman machine inspect --format '{{.State}}' 2>/dev/null || true)
    if [ -n "${running:-}" ] && [ "${running:-}" != "running" ]; then
      echo "ERROR: podman machine is not running. Start it with: podman machine start" >&2
      exit 1
    fi
  fi

  machine_socket=$(podman machine inspect --format '{{.ConnectionInfo.PodmanSocket.Path}}' 2>/dev/null || true)
  if [ -n "${machine_socket:-}" ] && [ "$machine_socket" != "<no value>" ]; then
    DOCKER_HOST="unix://$machine_socket"
  else
    socket_path=$(podman info --format '{{.Host.RemoteSocket.Path}}' 2>/dev/null || true)
    if [ -n "${socket_path:-}" ] && [ "$socket_path" != "<no value>" ]; then
      case "$socket_path" in
        unix://*) DOCKER_HOST="$socket_path" ;;
        /*)       DOCKER_HOST="unix://$socket_path" ;;
      esac
    fi
  fi
fi

if [ -z "${DOCKER_HOST:-}" ]; then
  echo "ERROR: could not detect a podman socket" >&2
  exit 1
fi

export DOCKER_HOST
SOCKET_PATH="${DOCKER_HOST#unix://}"

echo "Using container daemon: $DOCKER_HOST"

ACT_BIN=(act)
if ! act --version >/dev/null 2>&1; then
  if command -v mise >/dev/null 2>&1; then
    mise install act
    ACT_BIN=(mise x -- act)
  else
    echo "ERROR: act is not installed and mise is not available." >&2
    exit 1
  fi
fi

ACT_PLATFORM=(-P ubuntu-latest=ghcr.io/catthehacker/ubuntu:act-latest)
ARTIFACT_PATH="/tmp/standard-tools-cpp-act-artifacts"
mkdir -p "$ARTIFACT_PATH"
ACT_ARTIFACT=(--artifact-server-path "$ARTIFACT_PATH")

echo "Running quality job locally with act..."
set +e
"${ACT_BIN[@]}" push --defaultbranch main --job quality --container-daemon-socket "$SOCKET_PATH" "${ACT_PLATFORM[@]}" "${ACT_ARTIFACT[@]}" 2>&1 | tee act-quality.log
act_quality_exit=$?
set -e

perl -pe 's/\e\[[0-9;]*m//g; s/^\[[^\]]+\]\s*\|\s*//' act-quality.log \
  | grep -E '^(Test|.*tests.*passed|.*tests.*failed|FAILED|PASSED|Build|Error)' > test-output.log 2>/dev/null || true
if [ "$act_quality_exit" -ne 0 ]; then
  echo "FAIL act quality job" >> test-output.log
  echo "ERROR: act quality job failed" >&2
fi
./scripts/visual-test-report.sh test-output.log test-report.html
if [ "$act_quality_exit" -ne 0 ]; then
  exit 1
fi

DB_CONTAINER="stcpp-postgres"
"${ENGINE_CMD[@]}" rm -f "$DB_CONTAINER" >/dev/null 2>&1 || true
"${ENGINE_CMD[@]}" run -d --name "$DB_CONTAINER" \
  -e POSTGRES_USER=postgres \
  -e POSTGRES_PASSWORD=postgres \
  -e POSTGRES_DB=standard_tools \
  -P \
  postgres:17

cleanup_db() {
  "${ENGINE_CMD[@]}" rm -f "$DB_CONTAINER" >/dev/null 2>&1 || true
}
trap cleanup_db EXIT INT TERM

db_port=$("${ENGINE_CMD[@]}" port "$DB_CONTAINER" 5432/tcp | head -n1 | awk -F: '{print $NF}')
export SQT_DATABASE_URL="postgres://postgres:postgres@localhost:${db_port}/standard_tools?sslmode=disable"

echo "Waiting for PostgreSQL to be ready..."
db_ready=0
for i in {1..30}; do
  if "${ENGINE_CMD[@]}" exec "$DB_CONTAINER" pg_isready -U postgres >/dev/null 2>&1; then
    db_ready=1
    break
  fi
  sleep 1
done

if [ "$db_ready" -ne 1 ]; then
  echo "ERROR: PostgreSQL did not become ready in time" >&2
  exit 1
fi

echo "Running integration tests..."
set +e
cmake -S . -B build -DSTANDARD_TOOLS_BUILD_INTEGRATION_TESTS=ON -DSTANDARD_TOOLS_BUILD_E2E_TESTS=OFF >/dev/null
cmake --build build --target integration_tests --parallel $(nproc) >/dev/null 2>&1
ctest --test-dir build -R integration_tests --output-on-failure 2>&1 | tee test-integration.log
integration_exit=$?
set -e

./scripts/visual-test-report.sh test-integration.log test-report-integration.html

echo "Building and smoke testing images with podman..."
"${ENGINE_CMD[@]}" build --format docker -f Dockerfile -t standard-tools-cpp:latest .
./scripts/smoke-test-image.sh standard-tools-cpp:latest

"${ENGINE_CMD[@]}" build --format docker -f Dockerfile.native -t standard-tools-cpp:native .
./scripts/smoke-test-image.sh standard-tools-cpp:native

if [ -f test-report.html ]; then
  cp test-report.html test-report-local.html
  echo "Local unit-test report copied to test-report-local.html"
fi
if [ -f test-report-integration.html ]; then
  cp test-report-integration.html test-report-integration-local.html
  echo "Local integration-test report copied to test-report-integration-local.html"
fi

if [ "$integration_exit" -ne 0 ]; then
  echo "ERROR: integration tests failed" >&2
  exit 1
fi

echo "All local checks passed."
