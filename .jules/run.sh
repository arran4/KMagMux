#!/bin/bash
set -euo pipefail

ROOTFS_DIR="${HOME}/.cache/kde-dev-rootfs/rootfs"

if [ ! -d "$ROOTFS_DIR" ]; then
    echo "Rootfs not found at $ROOTFS_DIR. Run .jules/bootstrap.sh first."
    exit 1
fi

WORKSPACE_DIR="$(pwd)"

# Ensure the rootfs has a workspace directory
sudo mkdir -p "${ROOTFS_DIR}/workspace"

# Mounts array for cleanup
declare -a MOUNTS=()

cleanup() {
    for MNT in "${MOUNTS[@]}"; do
        if mountpoint -q "$MNT"; then
            sudo umount -l "$MNT" || true
        fi
    done
}
trap cleanup EXIT

do_mount() {
    local src="$1"
    local dst="$2"
    local opts="${3:-}"

    sudo mkdir -p "$dst"
    if ! mountpoint -q "$dst"; then
        if [ -n "$opts" ]; then
            sudo mount $opts "$src" "$dst"
        else
            sudo mount --bind "$src" "$dst"
        fi
        # Prepend to array to unmount in reverse order
        MOUNTS=("$dst" "${MOUNTS[@]}")
    fi
}

do_mount proc "${ROOTFS_DIR}/proc" "-t proc"
do_mount sysfs "${ROOTFS_DIR}/sys" "-t sysfs"
do_mount devtmpfs "${ROOTFS_DIR}/dev" "-t devtmpfs"
do_mount devpts "${ROOTFS_DIR}/dev/pts" "-t devpts"
do_mount "$WORKSPACE_DIR" "${ROOTFS_DIR}/workspace"

# Copy resolv.conf to rootfs if not already present or symlink
if [ -L "${ROOTFS_DIR}/etc/resolv.conf" ]; then
    sudo rm -f "${ROOTFS_DIR}/etc/resolv.conf"
fi
sudo cp /etc/resolv.conf "${ROOTFS_DIR}/etc/resolv.conf"

# Execute command via chroot
sudo chroot "$ROOTFS_DIR" /usr/bin/env -i \
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    HOME=/workspace \
    TERM="${TERM:-xterm}" \
    QT_QPA_PLATFORM="offscreen" \
    /bin/bash -c "cd /workspace && $*"
