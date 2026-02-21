#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/config.sh"

ROOTFS_DIR="${OUTPUT_DIR}/rootfs"
STAGING_DIR="${OUTPUT_DIR}/staging"

echo "========================================="
echo "  Building Root Filesystem"
echo "========================================="

create_directory_structure() {
    echo "Creating directory structure..."
    
    mkdir -p "${ROOTFS_DIR}"/{bin,boot,dev,etc,home,lib,media,mnt,opt,proc,root,run,sbin,srv,tmp,usr,var}
    mkdir -p "${ROOTFS_DIR}"/usr/{bin,lib,sbin,share,include}
    mkdir -p "${ROOTFS_DIR}"/var/{{lib,log,cache,www}/nginx,lib/php,run}
    mkdir -p "${ROOTFS_DIR}"/etc/{nginx,sysconfig,systemd}
    mkdir -p "${ROOTFS_DIR}"/boot/grub
    mkdir -p "${ROOTFS_DIR}"/root/{.config,.local}
    mkdir -p "${ROOTFS_DIR}"/home/"${DEFAULT_USER}"/{Documents,Downloads,.config}
    mkdir -p "${ROOTFS_DIR}"/srv/{http,https}
    mkdir -p "${ROOTFS_DIR}"/mnt/{data,persist}
    
    mkdir -p "${ROOTFS_DIR}"/opt/xiaokang
    mkdir -p "${ROOTFS_DIR}"/www/html
    
    echo "Done."
}

install_basic_files() {
    echo "Installing basic system files..."
    
    cat > "${ROOTFS_DIR}/etc/hostname" << EOF
${SYSTEM_HOSTNAME}
EOF

    cat > "${ROOTFS_DIR}/etc/hosts" << EOF
127.0.0.1   localhost
127.0.1.1   ${SYSTEM_HOSTNAME}

# The following lines are desirable for IPV6 capable hosts
::1     ip6-localhost ip6-loopback
fe00::0 ip6-localnet
ff00::0 ip6-mcastprefix
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters
EOF

    cat > "${ROOTFS_DIR}/etc/passwd" << EOF
root:x:0:0:root:/root:/bin/sh
${DEFAULT_USER}:x:1000:1000:${DEFAULT_USER}:/home/${DEFAULT_USER}:/bin/sh
nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
www-data:x:33:33:www-data:/var/www:/usr/sbin/nologin
EOF

    cat > "${ROOTFS_DIR}/etc/group" << EOF
root:x:0:
${DEFAULT_USER}:x:1000:
audio:x:29:
video:x:44:
www-data:x:33:
EOF

    cat > "${ROOTFS_DIR}/etc/fstab" << EOF
# /etc/fstab: static file system information
overlay / overlay defaults 0 0
proc /proc proc defaults 0 0
sysfs /sys sysfs defaults 0 0
devpts /dev/pts devpts defaults 0 0
tmpfs /tmp tmpfs defaults,size=512M 0 0
EOF

    cat > "${ROOTFS_DIR}/etc/profile" << EOF
export PATH=/usr/local/bin:/usr/bin:/bin:/sbin:/usr/sbin
export HOME=/root
export TERM=xterm-256color
PS1='\[\033[01;32m\]\u@\h\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$ '
alias ll='ls -la'
EOF

    echo "Done."
}

install_systemd() {
    echo "Installing systemd service files..."
    
    cat > "${ROOTFS_DIR}/etc/systemd/system/getty@tty1.service.d/override.conf" << 'EOF'
[Service]
Type=idle
ExecStart=
ExecStart=-/sbin/agetty --autologin root --noclear %I $TERM
EOF

    mkdir -p "${ROOTFS_DIR}/etc/systemd/system/getty@tty1.service.d"
    
    cat > "${ROOTFS_DIR}/etc/systemd/system/xiaokang-browser.service" << 'EOF'
[Unit]
Description=XiaoKangOS Browser Service
After=graphical.target
Requires=graphical.target

[Service]
Type=simple
User=xiao
Group=xiao
Environment=DISPLAY=:0
Environment=XDG_RUNTIME_DIR=/run/user/1000
ExecStartPre=/bin/sleep 5
ExecStart=/opt/xiaokang/browser/xiaokang-browser --kiosk --no-first-run --disable-infobars http://localhost/
Restart=on-failure
RestartSec=5

[Install]
WantedBy=graphical.target
EOF

    cat > "${ROOTFS_DIR}/etc/systemd/system/xiaokang-php-fpm.service" << 'EOF'
[Unit]
Description=XiaoKangOS PHP-FPM Service
After=network.target

[Service]
Type=notify
PIDFile=/run/php/php-fpm.pid
ExecStart=/usr/sbin/php-fpm --nodaemonize --fpm-config /etc/php/8.1/fpm/pool.d/www.conf
Restart=always

[Install]
WantedBy=multi-user.target
EOF

    cat > "${ROOTFS_DIR}/etc/systemd/system/xiaokang-nginx.service" << 'EOF'
[Unit]
Description=XiaoKangOS Nginx Service
After=network.target xiaokang-php-fpm.service
Requires=xiaokang-php-fpm.service

[Service]
Type=forking
PIDFile=/run/nginx/pid
ExecStartPre=/usr/sbin/nginx -t
ExecStart=/usr/sbin/nginx
ExecReload=/bin/kill -s HUP $MAINPID
ExecStop=/bin/kill -s QUIT $MAINPID

[Install]
WantedBy=multi-user.target
EOF

    echo "Done."
}

configure_kernel_modules() {
    echo "Configuring kernel modules..."
    
    cat > "${ROOTFS_DIR}/etc/modules" << 'EOF'
# Load kernel modules at boot
loop
squashfs
overlay
aufs
EOF

    cat > "${ROOTFS_DIR}/etc/modprobe.d/blacklist.conf" << 'EOF'
blacklist pcspkr
blacklist snd_pcsp
EOF

    echo "Done."
}

configure_network() {
    echo "Configuring network..."
    
    cat > "${ROOTFS_DIR}/etc/resolv.conf" << 'EOF'
nameserver 8.8.8.8
nameserver 8.8.4.4
EOF

    cat > "${ROOTFS_DIR}/etc/systemd/systemd-networkd" << 'EOF'
[Match]
Name=*

[Network]
DHCP=yes
IPv6AcceptRA=yes
EOF

    mkdir -p "${ROOTFS_DIR}/etc/systemd/network"
    cat > "${ROOTFS_DIR}/etc/systemd/network/99-default.network" << 'EOF'
[Match]
Name=*

[Network]
DHCP=ipv4
EOF

    echo "Done."
}

setup_persistence() {
    echo "Setting up persistence..."
    
    cat > "${ROOTFS_DIR}/etc/fstab" << 'EOF'
overlay / overlay defaults 0 0
proc /proc proc defaults 0 0
sysfs /sys sysfs defaults 0 0
devpts /dev/pts devpts defaults 0 0
tmpfs /tmp tmpfs defaults,size=512M 0 0
/dev/sda1 /mnt/persist ext4 defaults 0 0
EOF

    mkdir -p "${ROOTFS_DIR}/mnt/persist"
    mkdir -p "${ROOTFS_DIR}/var/lib/browser-data"
    
    cat > "${ROOTFS_DIR}/etc/systemd/system/persist-data.mount" << 'EOF'
[Unit]
Description=Persistent Data Mount
RequiresMountsFor=/mnt/persist

[Mount]
What=/dev/sda1
Where=/mnt/persist
Type=ext4
Options=defaults

[Install]
WantedBy=multi-user.target
EOF

    echo "Done."
}

main() {
    create_directory_structure
    install_basic_files
    install_systemd
    configure_kernel_modules
    configure_network
    setup_persistence
    
    echo "========================================="
    echo "  Root filesystem created successfully"
    echo "========================================="
}

main
