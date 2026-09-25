#!/bin/bash
# build_libnx32.sh -- the AArch32 libnx this project links: vita2hos/libnx at
# 721c977 ("AArch32 support", what the vita2hos image carries) plus the fixes
# on this branch (git log 721c977..), installed into ./prefix.
# Run inside the toolchain container (as gfx32/build_gfx32.sh):
#   docker run --rm --platform linux/amd64 -v "$PWD:/work" -w /work \
#     ghcr.io/vita2hos/devcontainer/vita2hos:latest bash -lc ./build_libnx32.sh
# Projects then mount ./prefix over the image's $DEVKITPRO/libnx32
# (pvztouch_nx/build.sh does). tools32/check_short_enums.sh re-checks the
# headers for IPC layouts that depend on the size of an enum.
set -euo pipefail
cd "$(dirname "$0")"
make -C nx -f Makefile.32 -j"$(nproc)" dist-bin
rm -rf prefix && mkdir prefix
tar -xjf nx/libnx-*.tar.bz2 -C prefix
rm -f nx/libnx-*.tar.bz2
ls -l prefix/lib
