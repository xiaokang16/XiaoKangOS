#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

ISO_DIR="${OUTPUT_DIR}/iso"
ROOTFS_DIR="${OUTPUT_DIR}/rootfs"
EFI_DIR="${ISO_DIR}/boot"

echo "========================================="
echo "  Creating XiaoKangOS ISO Image"
echo "========================================="

prepare_iso_structure() {
    echo "Preparing ISO structure..."
    
    mkdir -p "${ISO_DIR}"/{boot,casper,EFI,install,.disk}
    mkdir -p "${EFI_DIR}"/grub{x64,fonts}
    
    echo "Done."
}

create_grub_config() {
    echo "Creating GRUB configuration..."
    
    cat > "${EFI_DIR}/grub/grub.cfg" << 'EOF'
set default=0
set timeout=0
set timeout_style=hidden

# Load graphics modules
insmod all_video
insmod gfxterm
insmod png

# Set graphics mode
set gfxmode=auto
terminal_output gfxterm

# Menu colors
set menu_color_normal=white/black
set menu_color_highlight=black/light-gray
set color_normal=white/black

menuentry "XiaoKangOS - 启动系统" --class xiaokangos {
    load_video
    set gfxpayload=keep
    linux /boot/vmlinuz quiet splash root=/dev/cowfs rootflags=overlay --verbose
    initrd /boot/initrd.img
}

menuentry "XiaoKangOS - 高级选项" --class xiaokangos {
    load_video
    set gfxpayload=keep
    linux /boot/vmlinuz quiet splash root=/dev/cowfs rootflags=overlay single
    initrd /boot/initrd.img
}

menuentry "XiaoKangOS - 恢复模式" --class xiaokangos {
    load_video
    set gfxpayload=keep
    linux /boot/vmlinuz nomodeset root=/dev/cowfs rootflags=overlay recovery
    initrd /boot/initrd.img
}
EOF

    cat > "${EFI_DIR}/grub/grub-efi.cfg" << 'EOF'
set default=0
set timeout=0

menuentry "XiaoKangOS" {
    search --no-floppy --fs-uuid --set=root
    linux /boot/vmlinuz quiet splash root=LABEL=XIAOKANGOS
    initrd /boot/initrd.img
}
EOF

    echo "Done."
}

create_efi_boot() {
    echo "Creating EFI boot files..."
    
    if command -v grub-mkimage &> /dev/null; then
        grub-mkimage -O x86_64-efi -o "${EFI_DIR}/grubx64/shimx64.efi" -p '' boot linux efi_gop
        grub-mkimage -O x86_64-efi -o "${EFI_DIR}/grubx64/grubx64.efi" -p '' boot linux efi_gop
    fi
    
    cp /usr/lib/shim/shimx64.efi.signed "${EFI_DIR}/grubx64/shimx64.efi" 2>/dev/null || \
    cp /usr/lib/shim/shimx64.efi "${EFI_DIR}/grubx64/shimx64.efi" 2>/dev/null || \
    echo "Note: EFI signed shim not found, using fallback"
    
    echo "Done."
}

create_isolinux_config() {
    echo "Creating Syslinux/ISOLINUX configuration..."
    
    mkdir -p "${ISO_DIR}/boot/syslinux"
    
    cat > "${ISO_DIR}/boot/syslinux/isolinux.cfg" << 'EOF'
UI vesamenu.c32
MENU TITLE XiaoKangOS Boot Menu
MENU BACKGROUND splash.png

DEFAULT xiaokang
TIMEOUT 0
PROMPT 0

LABEL xiaokang
    MENU LABEL XiaoKangOS - Start System
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initrd.img quiet splash root=/dev/cowfs rootflags=overlay --

LABEL advanced
    MENU LABEL XiaoKangOS - Advanced Options
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initrd.img quiet splash root=/dev/cowfs rootflags=overlay single --

LABEL recovery
    MENU LABEL XiaoKangOS - Recovery Mode
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initrd.img nomodeset root=/dev/cowfs recovery --
EOF

    if [ -f /usr/lib/ISOLINUX/isolinux.bin ]; then
        cp /usr/lib/ISOLINUX/isolinux.bin "${ISO_DIR}/boot/syslinux/"
        cp /usr/lib/ISOLINUX/vesamenu.c32 "${ISO_DIR}/boot/syslinux/" 2>/dev/null
        cp /usr/lib/syslinux/modules/bios/*.c32 "${ISO_DIR}/boot/syslinux/" 2>/dev/null
    fi
    
    echo "Done."
}

copy_kernel_and_initrd() {
    echo "Copying kernel and initrd..."
    
    if [ -f "${OUTPUT_DIR}/vmlinuz" ]; then
        cp "${OUTPUT_DIR}/vmlinuz" "${ISO_DIR}/boot/"
    else
        echo "WARNING: Kernel not found at ${OUTPUT_DIR}/vmlinuz"
    fi
    
    if [ -f "${ISO_DIR}/boot/initrd.img" ]; then
        echo "Initrd already exists"
    else
        echo "Creating initrd..."
        if [ -d "${ROOTFS_DIR}" ]; then
            cd "${ROOTFS_DIR}"
            find . -print0 | cpio -0 -o -H newc 2>/dev/null | gzip -9 > "${ISO_DIR}/boot/initrd.img"
        fi
    fi
    
    echo "Done."
}

create_squashfs() {
    echo "Creating SquashFS filesystem..."
    
    if [ -d "${ROOTFS_DIR}" ]; then
        mksquashfs "${ROOTFS_DIR}" "${ISO_DIR}/casper/filesystem.squashfs" \
            -comp xz \
            -b 1M \
            -e boot \
            -e proc \
            -e sys \
            -e dev \
            -e run \
            -e tmp \
            || echo "Warning: mksquashfs failed, continuing..."
    fi
    
    echo "Done."
}

create_manifest() {
    echo "Creating manifest..."
    
    if [ -f "${ISO_DIR}/casper/filesystem.squashfs" ]; then
        du -sx "${ROOTFS_DIR}" > "${ISO_DIR}/casper/filesystem.size"
    fi
    
    cat > "${ISO_DIR}/casper/filesystem.manifest" << 'EOF'
# XiaoKangOS System Packages
# This file lists the packages included in the live system
EOF

    echo "Done."
}

create_disk_info() {
    echo "Creating disk info..."
    
    cat > "${ISO_DIR}/.disk/info" << EOF
XiaoKangOS ${SYSTEM_VERSION} ${ARCH}
Build date: $(date -u +"%Y-%m-%d %H:%M UTC")
Kernel: Linux ${KERNEL_VERSION}
Architecture: ${ARCH}
EOF

    touch "${ISO_DIR}/.disk/base_installable"
    touch "${ISO_DIR}/.disk/cdrom_installed"
    
    echo "Done."
}

generate_iso() {
    echo "Generating ISO image..."
    
    local iso_file="${OUTPUT_DIR}/XiaoKangOS-${SYSTEM_VERSION}.iso"
    
    xorriso -as mkisofs \
        -iso-level 3 \
        -full-iso9660-filenames \
        -volid "${ISO_VOLUME}" \
        -publisher "${ISO_PUBLISHER}" \
        -app-id "XiaoKangOS" \
        -verbose \
        -output "${iso_file}" \
        -eltorito-catalog boot/syslinux/isolinux.bin \
        -eltorito-boot boot/syslinux/isolinux.bin \
        -no-emul-boot \
        -boot-load-size 4 \
        -boot-info-table \
        -eltorito-alt-boot \
        -efi-boot boot/grub/shimx64.efi \
        -no-emul-boot \
        "${ISO_DIR}" \
        || {
            echo "ISO with EFI failed, trying without EFI..."
            xorriso -as mkisofs \
                -iso-level 3 \
                -full-iso9660-filenames \
                -volid "${ISO_VOLUME}" \
                -publisher "${ISO_PUBLISHER}" \
                -app-id "XiaoKangOS" \
                -output "${iso_file}" \
                -eltorito-catalog boot/syslinux/isolinux.bin \
                -eltorito-boot boot/syslinux/isolinux.bin \
                -no-emul-boot \
                -boot-load-size 4 \
                -boot-info-table \
                "${ISO_DIR}"
        }
    
    echo "========================================="
    echo "  ISO Image Created Successfully!"
    echo "========================================="
    echo "File: ${iso_file}"
    ls -lh "${iso_file}"
    
    if command -v isohybrid &> /dev/null; then
        isohybrid "${iso_file}" 2>/dev/null || echo "Note: isohybrid not applicable"
    fi
    
    echo "========================================="
}

main() {
    prepare_iso_structure
    create_grub_config
    create_isolinux_config
    copy_kernel_and_initrd
    create_squashfs
    create_manifest
    create_disk_info
    generate_iso
}

main
