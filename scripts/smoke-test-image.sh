#!/usr/bin/env bash
# Smoke-tests a container image by running it with podman, verifying image
# metadata, waiting for the HTTP /health endpoint, then checking the gRPC health
# endpoint.
set -euo pipefail

cd "$(dirname "$0")/.."

command -v curl >/dev/null 2>&1 || { echo "curl is required for smoke tests" >&2; exit 1; }

read -ra ENGINE_CMD <<< "${ENGINE:-podman}"
command -v "${ENGINE_CMD[0]}" >/dev/null 2>&1 || { echo "Container engine ${ENGINE_CMD[0]} is required" >&2; exit 1; }

IMAGE_TAG="${1:-}"
if [ -z "$IMAGE_TAG" ]; then
  echo "usage: $0 <image-tag>" >&2
  exit 2
fi

container_name="stcpp-smoke-${IMAGE_TAG//[:\/]/-}"

# The gRPC health checker is shipped inside the image at /usr/local/bin/grpc_health_check;
# we invoke it via the container engine so the host does not need the C++ build toolchain.
cleanup() {
  local exit_code=$?
  if [ "$exit_code" -ne 0 ]; then
    echo "Smoke test failed with exit code $exit_code; dumping container logs:" >&2
    "${ENGINE_CMD[@]}" logs "$container_name" >&2 || true
  fi
  "${ENGINE_CMD[@]}" rm -f "$container_name" >/dev/null 2>&1 || true
  exit $exit_code
}
trap cleanup EXIT INT TERM

echo "Inspecting image metadata for $IMAGE_TAG..."
image_user=$("${ENGINE_CMD[@]}" inspect "$IMAGE_TAG" --format '{{.Config.User}}')
case "$image_user" in
  nonroot|nonroot:nonroot|65532|65532:65532) ;;
  *) echo "ERROR: unexpected image user '$image_user' (expected nonroot or 65532)" >&2; exit 1 ;;
esac

image_exposed=$("${ENGINE_CMD[@]}" inspect "$IMAGE_TAG" --format '{{json .Config.ExposedPorts}}')
for port in 8080 50051; do
  if ! echo "$image_exposed" | grep -q "\"${port}/tcp\""; then
    echo "ERROR: port ${port}/tcp is not exposed" >&2
    exit 1
  fi
done

if command -v jq >/dev/null 2>&1; then
  if ! "${ENGINE_CMD[@]}" inspect "$IMAGE_TAG" | jq -e '.[0].Healthcheck' >/dev/null 2>&1; then
    echo "ERROR: image has no HEALTHCHECK configured" >&2
    exit 1
  fi
else
  if ! "${ENGINE_CMD[@]}" inspect "$IMAGE_TAG" | grep -q '"Healthcheck"'; then
    echo "ERROR: image has no HEALTHCHECK configured" >&2
    exit 1
  fi
fi
echo "Image metadata OK"

"${ENGINE_CMD[@]}" rm -f "$container_name" >/dev/null 2>&1 || true
"${ENGINE_CMD[@]}" run -d -P --name "$container_name" "$IMAGE_TAG"

host_port=$("${ENGINE_CMD[@]}" port "$container_name" 8080/tcp | head -n1 | awk -F: '{print $NF}')
grpc_port=$("${ENGINE_CMD[@]}" port "$container_name" 50051/tcp | head -n1 | awk -F: '{print $NF}')

success=0
for i in {1..30}; do
  if curl -sf --connect-timeout 2 --max-time 5 "http://localhost:${host_port}/health"; then
    success=1
    break
  fi
  sleep 1
done

if [ "$success" -ne 1 ]; then
  echo "ERROR: HTTP health check failed for $IMAGE_TAG" >&2
  exit 1
fi

grpc_success=0
for i in {1..10}; do
  if "${ENGINE_CMD[@]}" exec "$container_name" /usr/local/bin/grpc_health_check "127.0.0.1:50051"; then
    grpc_success=1
    break
  fi
  sleep 1
done

if [ "$grpc_success" -ne 1 ]; then
  echo "ERROR: gRPC health check failed for $IMAGE_TAG" >&2
  exit 1
fi

echo "Health checks passed for $IMAGE_TAG"
