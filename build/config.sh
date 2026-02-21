#!/bin/bash

# XiaoKangOS Build Configuration
# This file contains all configuration variables for the build process

# Project paths
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL_DIR="${PROJECT_ROOT}/kernel"
BROWSER_DIR="${PROJECT_ROOT}/browser"
WEB_FRONTEND="${PROJECT_ROOT}/web/frontend"
WEB_BACKEND="${PROJECT_ROOT}/web/backend"
KERNEL_MODULE="${PROJECT_ROOT}/kernel-module"
OUTPUT_DIR="${PROJECT_ROOT Build}/output"

# settings
ARCH="x86_64"
CROSS_COMPILE=""
MAKE_JOBS=$(nproc)

# Kernel settings
KERNEL_VERSION="6.6.0"
KERNEL_CONFIG="xiaokang_defconfig"
KERNEL_COMPRESSION="lz4"

# Browser settings
CHROMIUM_VERSION="120.0.6099.109"
BROWSER_NAME="xiaokang-browser"
BROWSER_FLAGS="--kiosk --no-first-run --disable-infobars --disable-session-storage --disable-translate --no-default-browser-check"

# System settings
SYSTEM_NAME="XiaoKangOS"
SYSTEM_VERSION="1.0.0"
SYSTEM_HOSTNAME="xiaokang"

# User settings
DEFAULT_USER="xiao"
DEFAULT_PASSWORD="xiao"

# ISO settings
ISO_LABEL="XIAOKANGOS"
ISO_VOLUME="XiaoKangOS"
ISO_PUBLISHER="XiaoKangOS Team"
ISO_SIZE_LIMIT=1000

# Compression settings
ROOTFS_COMPRESSION="xz"
COMPRESSION_LEVEL=6

# Build options
ENABLE_NVIDIA=1
ENABLE_PHP=1
ENABLE_PHP_FPM=1
ENABLE_3D_SUPPORT=1

# Source URLs
KERNEL_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.tar.gz"
CHROMIUM_URL="https://commondatastorage.googleapis.com/chromium-browser-official/chromium-120.0.6099.109.tar.xz"

# Build host requirements
MIN_RAM_GB=8
MIN_DISK_GB=50

echo "========================================="
echo "  XiaoKangOS Build Configuration"
echo "========================================="
echo "Project Root: ${PROJECT_ROOT}"
echo "Architecture: ${ARCH}"
echo "Kernel Version: ${KERNEL_VERSION}"
echo "Chromium Version: ${CHROMIUM_VERSION}"
echo "System Version: ${SYSTEM_VERSION}"
echo "Make Jobs: ${MAKE_JOBS}"
echo "========================================="
