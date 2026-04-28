#!/usr/bin/env bash
# actions.sh — /usr/bin/actions.sh
# Usage: actions.sh <action> [args...]
set -euo pipefail

ACTION="${1:-help}"
shift || true

log() { echo "[actions] $*"; }
die() { echo "[actions] ERROR: $*" >&2; exit 1; }

# ════════════════════════════════════════════════════════
#  MAIN ACTIONS
# ════════════════════════════════════════════════════════
action_launch(){
    log "Launching mygame..."
    /usr/bin/start_mygame.sh
}
action_update(){
    log "Checking for updates..."
    # Implement OTA update logic here
    die "update not yet implemented"
}
action_shutdown(){
    log "Shutting down in 3s..."
    sleep 3 && shutdown -h now
}

# ════════════════════════════════════════════════════════
#  BLUETOOTH  (delegates to rocknix-bluetooth)
# ════════════════════════════════════════════════════════
BT="rocknix-bluetooth"

action_bt_on()           { $BT enable; }
action_bt_off()          { $BT disable; }
action_bt_scan()         { $BT start_live_devices; }
action_bt_scan_stop()    { $BT stop_live_devices; }
action_bt_list()         { $BT list; }
action_bt_pair()         { $BT trust "${1:?MAC required}"; }
action_bt_connect()      { $BT connect "${1:?MAC required}"; }
action_bt_disconnect()   { $BT disconnect "${1:?MAC required}"; }
action_bt_forget()       { $BT remove "${1:?MAC required}"; }
action_bt_audio_sink()   {
    # PipeWire sink name for connected BT audio device, if any
    pactl list short sinks 2>/dev/null | awk '/bluez/{print $2}'
}

# ════════════════════════════════════════════════════════
#  WI-FI  (iwd / iwctl)
# ════════════════════════════════════════════════════════
IWD_IFACE="${WIFI_IFACE:-wlan0}"
_iwctl(){ iwctl "$@" 2>/dev/null; }

action_wifi_on(){
    rfkill unblock wifi 2>/dev/null || true
    systemctl start iwd 2>/dev/null || true
    sleep 1
    _iwctl device "$IWD_IFACE" set-property Powered on
    log "Wi-Fi enabled."
}
action_wifi_off(){
    _iwctl device "$IWD_IFACE" set-property Powered off
    rfkill block wifi 2>/dev/null || true
    log "Wi-Fi disabled."
}
action_wifi_scan(){
    _iwctl station "$IWD_IFACE" scan
    sleep 2
    _iwctl station "$IWD_IFACE" get-networks
}
action_wifi_connect(){
    local SSID="${1:?SSID required}"
    local PASS="${2:-}"
    if [[ -n "$PASS" ]]; then
        local PROFILE_DIR="/var/lib/iwd"
        mkdir -p "$PROFILE_DIR"
        # iwd profile filename: spaces → underscores are NOT correct;
        # iwd uses the literal SSID as filename. Wrap in quotes for safety.
        local PROFILE_FILE="${PROFILE_DIR}/${SSID}.psk"
        cat > "${PROFILE_FILE}" <<EOF
[Security]
Passphrase=${PASS}
EOF
        chmod 600 "${PROFILE_FILE}"
    fi
    _iwctl station "$IWD_IFACE" connect "$SSID"
    if ! grep -q "EnableNetworkConfiguration.*true" /etc/iwd/main.conf 2>/dev/null; then
        sleep 2
        udhcpc -i "$IWD_IFACE" -n -q 2>/dev/null || dhclient "$IWD_IFACE" 2>/dev/null || true
    fi
    log "Connected to $SSID."
}
action_wifi_disconnect(){
    _iwctl station "$IWD_IFACE" disconnect
}
action_wifi_forget(){
    local SSID="${1:?SSID required}"
    rm -f "/var/lib/iwd/${SSID}.psk" \
          "/var/lib/iwd/${SSID}.8021x" \
          "/var/lib/iwd/${SSID}.open" 2>/dev/null || true
    _iwctl known-networks "$SSID" forget 2>/dev/null || true
    log "Forgotten: $SSID"
}

# ════════════════════════════════════════════════════════
#  USB GADGET  (ECM = ethernet / MTP = file transfer)
# ════════════════════════════════════════════════════════
action_usb_ecm(){
    usbgadget network
}
action_usb_mtp(){
    usbgadget file_transfer
}

# ════════════════════════════════════════════════════════
#  DISPATCH
# ════════════════════════════════════════════════════════
case "$ACTION" in
    launch)          action_launch             ;;
    update)          action_update             ;;
    shutdown)        action_shutdown           ;;
    bt_on)           action_bt_on               ;;
    bt_off)          action_bt_off              ;;
    bt_scan)         action_bt_scan             ;;
    bt_scan_stop)    action_bt_scan_stop        ;;
    bt_list)         action_bt_list             ;;
    bt_pair)         action_bt_pair        "$@" ;;
    bt_connect)      action_bt_connect     "$@" ;;
    bt_disconnect)   action_bt_disconnect  "$@" ;;
    bt_forget)       action_bt_forget      "$@" ;;
    bt_audio_sink)   action_bt_audio_sink       ;;
    wifi_on)         action_wifi_on            ;;
    wifi_off)        action_wifi_off           ;;
    wifi_scan)       action_wifi_scan          ;;
    wifi_connect)    action_wifi_connect  "$@" ;;
    wifi_disconnect) action_wifi_disconnect    ;;
    wifi_forget)     action_wifi_forget   "$@" ;;
    usb_ecm)         action_usb_ecm            ;;
    usb_mtp)         action_usb_mtp            ;;
    *)
        echo "Usage: $0 <action> [args]"
        echo "  launch | update | shutdown"
        echo "  bt_on | bt_off | bt_scan | bt_scan_stop | bt_list"
        echo "  bt_pair <mac> | bt_connect <mac> | bt_disconnect <mac>"
        echo "  bt_forget <mac> | bt_audio_sink"
        echo "  wifi_on | wifi_off | wifi_scan | wifi_connect <ssid> [pass]"
        echo "  wifi_disconnect | wifi_forget <ssid>"
        echo "  usb_ecm | usb_mtp"
        ;;
esac