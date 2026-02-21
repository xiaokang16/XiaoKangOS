#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

echo "========================================="
echo "  XiaoKangOS Build System"
echo "========================================="
echo ""

check_dependencies() {
    echo "[1/6] Checking build dependencies..."
    
    local deps=("make" "gcc" "bc" "bison" "flex" "xorriso" "genisoimage")
    local missing=()
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done
    
    if [ ${#missing[@]} -ne 0 ]; then
        echo "ERROR: Missing dependencies: ${missing[*]}"
        echo "Please install them with: sudo apt-get install ${missing[*]}"
        exit 1
    fi
    
    echo "All dependencies satisfied."
}

setup_environment() {
    echo "[2/6] Setting up build environment..."
    
    mkdir -p "${OUTPUT_DIR}"
    mkdir -p "${OUTPUT_DIR}/rootfs"
    mkdir -p "${OUTPUT_DIR}/iso"
    mkdir -p "${OUTPUT_DIR}/cache"
    
    echo "Environment ready."
}

download_sources() {
    echo "[3/6] Downloading sources..."
    
    if [ ! -d "${KERNEL_DIR}/linux-${KERNEL_VERSION}" ]; then
        echo "Downloading Linux kernel ${KERNEL_VERSION}..."
        mkdir -p "${KERNEL_DIR}"
        cd "${KERNEL_DIR}"
        if [ ! -f "linux-${KERNEL_VERSION}.tar.gz" ]; then
            wget -q "${KERNEL_URL}" -O "linux-${KERNEL_VERSION}.tar.gz" || curl -sL "${KERNEL_URL}" -o "linux-${KERNEL_VERSION}.tar.gz"
        fi
        echo "Extracting kernel..."
        tar -xzf "linux-${KERNEL_VERSION}.tar.gz"
    else
        echo "Kernel source already exists."
    fi
    
    echo "Sources ready."
}

build_kernel() {
    echo "[4/6] Building Linux kernel..."
    
    cd "${KERNEL_DIR}/linux-${KERNEL_VERSION}"
    
    if [ ! -f ".config" ]; then
        if [ -f "${KERNEL_DIR}/${KERNEL_CONFIG}" ]; then
            cp "${KERNEL_DIR}/${KERNEL_CONFIG}" .config
        else
            make "${KERNEL_CONFIG}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" || \
            make defconfig ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}"
        fi
    fi
    
    echo "Compiling kernel (this may take a while)..."
    make -j"${MAKE_JOBS}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" bzImage modules
    
    echo "Installing modules to staging..."
    make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" INSTALL_MOD_PATH="${OUTPUT_DIR}/rootfs" modules_install
    
    cp arch/x86/boot/bzImage "${OUTPUT_DIR}/vmlinuz"
    echo "Kernel build complete."
}

build_rootfs() {
    echo "[5/6] Building root filesystem..."
    
    local rootfs="${OUTPUT_DIR}/rootfs"
    
    echo "Creating basic directory structure..."
    mkdir -p "${rootfs}"/{bin,boot,dev,etc,home,lib,media,mnt,opt,proc,root,run,sbin,srv,sys,tmp,usr,var}
    mkdir -p "${rootfs}"/usr/{bin,lib,sbin,share}
    mkdir -p "${rootfs}"/var/{lib,log,cache,www}
    
    echo "Copying kernel modules..."
    if [ -d "${OUTPUT_DIR}/lib/modules" ]; then
        cp -r "${OUTPUT_DIR}/lib/modules" "${rootfs}/lib/"
    fi
    
    echo "Installing busybox..."
    if ! command -v busybox &> /dev/null; then
        apt-get install -y busybox 2>/dev/null || echo "Please install busybox manually"
    fi
    if command -v busybox &> /dev/null; then
        cp "$(command -v busybox)" "${rootfs}/bin/"
        for cmd in sh ash mount umount ls mkdir rm echo cat; do
            ln -sf busybox "${rootfs}/bin/${cmd}"
        done
    fi
    
    echo "Root filesystem structure created."
}

build_iso() {
    echo "[6/6] Building ISO image..."
    
    local iso_dir="${OUTPUT_DIR}/iso"
    local rootfs="${OUTPUT_DIR}/rootfs"
    local iso_output="${OUTPUT_DIR}/XiaoKangOS.iso"
    
    echo "Preparing ISO structure..."
    mkdir -p "${iso_dir}/boot"
    mkdir -p "${iso_dir}/boot/grub"
    
    cp "${OUTPUT_DIR}/vmlinuz" "${iso_dir}/boot/"
    
    # 创建 GRUB 配置文件
    cat > "${iso_dir}/boot/grub/grub.cfg" << 'EOF'
set default=0
set timeout=0

menuentry "XiaoKangOS" {
    linux /boot/vmlinuz quiet splash root=/dev/ram0
    initrd /boot/initrd.img
}
EOF

    echo "Creating initrd..."
    cd "${rootfs}"
    find . -print | cpio -o -H newc 2>/dev/null | gzip -9 > "${iso_dir}/boot/initrd.img"
    
    echo "Creating SquashFS..."
    mkdir -p "${iso_dir}/casper"
mksquashfs "${rootfs}" "${iso_dir}/casper/filesystem.squashfs" -comp xz -e boot -noappend
    
    echo "Creating ISO image with grub-mkrescue..."
    grub-mkrescue -o "${iso_output}" "${iso_dir}" -- -volid "${ISO_VOLUME}"
    
    echo "========================================="
    echo "  Build Complete!"
    echo "========================================="
    echo "ISO: ${iso_output}"
    ls -lh "${iso_output}"
}

main() {
    check_dependencies
    setup_environment
    download_sources
    build_kernel
    build_rootfs
    build_iso
}

if [ "$1" == "clean" ]; then
    echo "Cleaning build artifacts..."
    rm -rf "${OUTPUT_DIR}"
    echo "Clean complete."
elif [ "$1" == "kernel" ]; then
    check_dependencies
    setup_environment
    download_sources
    build_kernel
elif [ "$1" == "rootfs" ]; then
    setup_environment
    build_rootfs
elif [ "$1" == "iso" ]; then
    build_iso
else
    main
fi
