#!/bin/bash

# XiaoKangOS Session Persistence Manager
# Handles saving and restoring browser session state

PERSIST_DIR="/mnt/persist"
BROWSER_DATA_DIR="${PERSIST_DIR}/browser-data"
SESSION_FILE="${BROWSER_DATA_DIR}/session.json"
LOCALSTORAGE_DIR="${BROWSER_DATA_DIR}/localStorage"
COOKIES_FILE="${BROWSER_DATA_DIR}/cookies.sqlite"
SETTINGS_FILE="${BROWSER_DATA_DIR}/settings.json"

log() {
    echo "[SessionManager] $*"
}

init_persistence() {
    log "Initializing persistence storage..."
    
    mkdir -p "${BROWSER_DATA_DIR}"
    mkdir -p "${LOCALSTORAGE_DIR}"
    
    if [ ! -f "${SETTINGS_FILE}" ]; then
        cat > "${SETTINGS_FILE}" << 'EOF'
{
  "theme": "dark",
  "animationsEnabled": true,
  "refreshInterval": 5000,
  "autoStartBrowser": true,
  "showNotifications": true,
  "language": "zh-CN"
}
EOF
    fi
    
    log "Persistence storage initialized"
}

save_session() {
    local url="$1"
    local title="$2"
    local timestamp=$(date +%s)
    
    log "Saving session: ${url}"
    
    mkdir -p "${BROWSER_DATA_DIR}"
    
    local session_data=$(cat << EOF
{
  "url": "${url}",
  "title": "${title}",
  "timestamp": ${timestamp},
  "scrollPosition": 0,
  "formData": {}
}
EOF
)
    
    echo "${session_data}" > "${SESSION_FILE}"
    log "Session saved successfully"
}

restore_session() {
    if [ -f "${SESSION_FILE}" ]; then
        log "Restoring previous session..."
        
        local url=$(grep -o '"url": *"[^"]*"' "${SESSION_FILE}" | cut -d'"' -f4)
        
        if [ -n "${url}" ]; then
            log "Restored URL: ${url}"
            echo "${url}"
            return 0
        fi
    fi
    
    log "No session to restore, using default"
    return 1
}

save_localstorage() {
    local domain="$1"
    local data="$2"
    
    local domain_dir="${LOCALSTORAGE_DIR}/${domain}"
    mkdir -p "${domain_dir}"
    
    echo "${data}" > "${domain_dir}/data.json"
    log "LocalStorage saved for domain: ${domain}"
}

load_localstorage() {
    local domain="$1"
    
    local file="${LOCALSTORAGE_DIR}/${domain}/data.json"
    
    if [ -f "${file}" ]; then
        cat "${file}"
        return 0
    fi
    
    return 1
}

save_cookies() {
    local cookies="$1"
    
    echo "${cookies}" > "${COOKIES_FILE}"
    log "Cookies saved"
}

load_cookies() {
    if [ -f "${COOKIES_FILE}" ]; then
        cat "${COOKIES_FILE}"
        return 0
    fi
    
    return 1
}

save_settings() {
    local settings="$1"
    
    echo "${settings}" > "${SETTINGS_FILE}"
    log "Settings saved"
}

load_settings() {
    if [ -f "${SETTINGS_FILE}" ]; then
        cat "${SETTINGS_FILE}"
        return 0
    fi
    
    return 1
}

clear_session() {
    log "Clearing session data..."
    
    rm -rf "${BROWSER_DATA_DIR}"
    mkdir -p "${BROWSER_DATA_DIR}"
    mkdir -p "${LOCALSTORAGE_DIR}"
    
    log "Session data cleared"
}

case "$1" in
    init)
        init_persistence
        ;;
    save)
        save_session "$2" "$3"
        ;;
    restore)
        restore_session
        ;;
    save-ls)
        save_localstorage "$2" "$3"
        ;;
    load-ls)
        load_localstorage "$2"
        ;;
    save-cookies)
        save_cookies "$2"
        ;;
    load-cookies)
        load_cookies
        ;;
    save-settings)
        save_settings "$2"
        ;;
    load-settings)
        load_settings
        ;;
    clear)
        clear_session
        ;;
    *)
        echo "Usage: $0 {init|save|restore|save-ls|load-ls|save-cookies|load-cookies|save-settings|load-settings|clear}"
        exit 1
        ;;
esac
