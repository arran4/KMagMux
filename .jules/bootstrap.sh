#!/bin/bash
set -euo pipefail

ROOTFS_CACHE_DIR="${HOME}/.cache/kde-dev-rootfs"
MARKER_FILE="${ROOTFS_CACHE_DIR}/.ready"
ARCHIVE_URL="https://github.com/arran4/kde-dev-rootfs/releases/latest/download/kde-dev-rootfs-forky-amd64.tar.zst"
SHA256_URL="https://github.com/arran4/kde-dev-rootfs/releases/latest/download/kde-dev-rootfs-forky-amd64.tar.zst.sha256"
ARCHIVE_FILE="${ROOTFS_CACHE_DIR}/kde-dev-rootfs-forky-amd64.tar.zst"
SHA256_FILE="${ROOTFS_CACHE_DIR}/kde-dev-rootfs-forky-amd64.tar.zst.sha256"
ROOTFS_DIR="${ROOTFS_CACHE_DIR}/rootfs"

if [ -f "$MARKER_FILE" ]; then
    echo "Rootfs already bootstrapped."
    exit 0
fi

mkdir -p "$ROOTFS_CACHE_DIR"

echo "Downloading archive..."
curl --fail --location --retry 3 -o "$ARCHIVE_FILE" "$ARCHIVE_URL"
echo "Downloading sha256..."
curl --fail --location --retry 3 -o "$SHA256_FILE" "$SHA256_URL"

cd "$ROOTFS_CACHE_DIR"
echo "Verifying checksum..."
EXPECTED=$(cat "$SHA256_FILE" | awk '{print $1}')
ACTUAL=$(sha256sum "$ARCHIVE_FILE" | awk '{print $1}')
if [ "$EXPECTED" != "$ACTUAL" ]; then
    echo "Checksums do not match! Expected $EXPECTED, got $ACTUAL"
    exit 1
fi

echo "Extracting rootfs..."
mkdir -p "$ROOTFS_DIR"
if ! command -v zstd > /dev/null; then
    sudo apt-get update && sudo apt-get install -y zstd
fi
sudo tar --numeric-owner -I zstd -xpf "$ARCHIVE_FILE" -C "$ROOTFS_DIR"

touch "$MARKER_FILE"
echo "Bootstrap complete."
