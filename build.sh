#!/bin/sh
# build.sh -- build libnx32 in the AArch32 toolchain image and install it into
# ./prefix. Set TOOLCHAIN_IMAGE to use another image.
set -e
IMAGE="${TOOLCHAIN_IMAGE:-ghcr.io/vita2hos/devcontainer/vita2hos:latest}"
HERE="$(cd "$(dirname "$0")" && pwd)"
exec docker run --rm --platform linux/amd64 -v "$HERE:/work" -w /work "$IMAGE" \
  bash -lc ./build_libnx32.sh
