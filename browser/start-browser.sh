#!/bin/bash

# XiaoKangOS Browser Launcher
# This script starts the Chromium browser in kiosk mode

BROWSER_NAME="xiaokang-browser"
BROWSER_BIN="/opt/xiaokang/browser/${BROWSER_NAME}"
DEFAULT_URL="http://localhost/"
KIOSK_URL="http://localhost/"

KIOSK_FLAGS=(
    "--kiosk"
    "--no-first-run"
    "--no-default-browser-check"
    "--disable-infobars"
    "--disable-session-storage"
    "--disable-translate"
    "--disable-extensions"
    "--disable-popup-blocking"
    "--disable-background-networking"
    "--disable-sync"
    "--disable-translate"
    "--metrics-recording-only"
    "--mute-audio"
    "--no-experiments"
    "--ignore-autoplay-restrictions"
    "--autoplay-policy=no-user-gesture-required"
    "--disable-features=TranslateUI"
    "--disable-ipc-flooding-protection"
    "--disable-renderer-backgrounding"
    "--enable-features=NetworkService,NetworkServiceInProcess"
    "--force-color-profile=srgb"
    "--disable-gpu-driver-bug-workarounds"
    "--enable-zero-copy"
    "--ignore-gpu-blocklist"
    "--enable-gpu-rasterization"
    "--enable-webgl"
    "--enable-accelerated-2d-canvas"
    "--enable-accelerated-video-decode"
    "--enable-hardware-overlays=single-fullscreen,single-on-top,underlay"
    "--use-gl=egl"
    "--enable-features=VaapiVideoDecoder"
    "--use-cmd-decoder=validating"
    "--enable-raw-draw"
    "--disable-raw-vid-backends"
    "--use-viz=0"
    "--disable-features=UseOzonePlatform"
    "--window-size=1920,1080"
    "--start-fullscreen"
    "--start-maximized"
)

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*"
}

error_exit() {
    log "ERROR: $1"
    exit 1
}

check_browser() {
    if [ ! -f "${BROWSER_BIN}" ]; then
        log "Browser binary not found at ${BROWSER_BIN}"
        log "Searching for Chromium..."
        
        local chromium_paths=(
            "/usr/bin/chromium"
            "/usr/bin/chromium-browser"
            "/usr/bin/google-chrome"
            "/opt/chromium/chromium"
            "/snap/bin/chromium"
        )
        
        for path in "${chromium_paths[@]}"; do
            if [ -f "$path" ]; then
                BROWSER_BIN="$path"
                log "Found browser at: ${BROWSER_BIN}"
                break
            fi
        done
        
        if [ ! -f "${BROWSER_BIN}" ]; then
            error_exit "No Chromium browser found. Please install chromium-browser"
        fi
    fi
}

setup_display() {
    if [ -z "$DISPLAY" ]; then
        export DISPLAY=:0
        log "Setting DISPLAY to ${DISPLAY}"
    fi
    
    if [ -z "$XAUTHORITY" ]; then
        export XAUTHORITY="$HOME/.Xauthority"
        log "Setting XAUTHORITY to ${XAUTHORITY}"
    fi
    
    export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
    mkdir -p "$XDG_RUNTIME_DIR"
}

wait_for_network() {
    log "Waiting for network..."
    local max_wait=30
    local count=0
    
    while [ $count -lt $max_wait ]; do
        if ping -c 1 -W 1 8.8.8.8 &>/dev/null || [ -f /sys/class/net/eth0/operstate ]; then
            log "Network is ready"
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    
    log "Warning: Network may not be available"
    return 0
}

wait_for_webserver() {
    log "Waiting for web server..."
    local max_wait=30
    local count=0
    
    while [ $count -lt $max_wait ]; do
        if curl -s -o /dev/null -w "%{http_code}" "${KIOSK_URL}" 2>/dev/null | grep -q "200\|302"; then
            log "Web server is ready"
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    
    log "Warning: Web server may not be available"
    return 0
}

start_browser() {
    log "Starting ${BROWSER_NAME} in kiosk mode..."
    log "URL: ${KIOSK_URL}"
    log "Flags: ${KIOSK_FLAGS[*]}"
    
    cd /opt/xiaokang/browser
    
    exec "${BROWSER_BIN}" "${KIOSK_FLAGS[@]}" "${KIOSK_URL}"
}

main() {
    log "XiaoKangOS Browser Service Starting..."
    
    check_browser
    setup_display
    wait_for_network
    wait_for_webserver
    start_browser
}

main "$@"
