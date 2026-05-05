#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

. /etc/profile

> "/storage/mygame/log.txt" && exec > >(tee "/storage/mygame/log.txt") 2>&1

GAMEDIR="/usr/share/mygame"
MUSIC_ACTIVE_FLAG="/var/run/songo-music-active"

# Signal to rocknix-fake-suspend that music is playing (lid close = screen off only)
touch "${MUSIC_ACTIVE_FLAG}"
trap "rm -f '${MUSIC_ACTIVE_FLAG}'" EXIT

# Setup volume indicator
: "${CFW_NAME:=SongoOS}"
USE_SONGO_VOL_TCP_SERVER="0"
SONGO_CFW_NAME="NONE"
if [[ "$CFW_NAME" = "muOS" ]] || [[ "$CFW_NAME" = "knulli" ]] || [[ "$CFW_NAME" = "SongoOS" ]]; then
    SONGO_CFW_NAME="${CFW_NAME}"
elif [ -f /mnt/SDCARD/.system/version.txt ] && grep -q "NextUI" /mnt/SDCARD/.system/version.txt; then
    SONGO_CFW_NAME="NextUI"
elif [ -f /mnt/SDCARD/spruce/spruce ]; then
    SONGO_CFW_NAME="Spruce"
fi
if [[ "$SONGO_CFW_NAME" != "NONE" ]]; then
    USE_SONGO_VOL_TCP_SERVER="1"
    if [ -f "${GAMEDIR}/runtime/volume-indicator/setup_vol_indicator" ]; then
        sh "${GAMEDIR}/runtime/volume-indicator/setup_vol_indicator" "${SONGO_CFW_NAME}"
    fi
fi
export SONGO_CFW_NAME
export USE_SONGO_VOL_TCP_SERVER

# Brightness detection
export SYSFS_BL_BRIGHTNESS="$(find /sys/class/backlight/*/ -name brightness 2>/dev/null | head -n 1)"
export SYSFS_BL_COMMAND="$(find /sys/kernel/debug/dispdbg/ -name command 2>/dev/null | head -n 1)"

if [ -n "${SYSFS_BL_BRIGHTNESS}" ]; then
  echo "Backlight TYPE2 detected!"
  export BL_TYPE="TYPE2"
  export SYSFS_BL_POWER="$(find /sys/class/backlight/*/ -name bl_power 2>/dev/null)"
  export SYSFS_BL_MAX="$(find /sys/class/backlight/*/ -name max_brightness 2>/dev/null | head -n 1)"
elif [ -n "${SYSFS_BL_COMMAND}" ]; then
  echo "Backlight TYPE1 detected!"
  export BL_TYPE="TYPE1"
  export SYSFS_BL_NAME="$(find /sys/kernel/debug/dispdbg/ -name name 2>/dev/null | head -n 1)"
  export SYSFS_BL_PARAM="$(find /sys/kernel/debug/dispdbg/ -name param 2>/dev/null | head -n 1)"
  export SYSFS_BL_START="$(find /sys/kernel/debug/dispdbg/ -name start 2>/dev/null | head -n 1)"
  export BL_COMMAND="setbl"
  export BL_NAME="lcd0"
else
  echo "Backlight objects not found!"
  export BL_TYPE="UNKNOWN"
fi

NO_BRIGHT_FADE_AVAILABLE='0'
SONGO_GET_BRIGHTNESS_PATH="${GAMEDIR}/runtime/brightness/default/get_brightness"
SONGO_SET_BRIGHTNESS_PATH="${GAMEDIR}/runtime/brightness/default/set_brightness"

if [[ "$BL_TYPE" = "TYPE1" ]] && [[ -e "${GAMEDIR}/runtime/brightness/SongoOS/get_brightness" ]]; then
  SONGO_GET_BRIGHTNESS_PATH="${GAMEDIR}/runtime/brightness/SongoOS/get_brightness"
fi

if [ "$BL_TYPE" = "UNKNOWN" ]; then
  NO_BRIGHT_FADE_AVAILABLE='1'
fi

export SONGO_GET_BRIGHTNESS_PATH
export SONGO_SET_BRIGHTNESS_PATH
export NO_BRIGHT_FADE_AVAILABLE

# Check and log GPU driver
CURRENT_DRIVER=$(gpudriver)
echo "Current GPU driver: '${CURRENT_DRIVER}'"
# Libmali shit so check if its Libmali or anything not Panfrost, problem will come on anything that uses Snapdragon
if ! echo "${CURRENT_DRIVER}" | grep -q panfrost; then
  echo "panfrost not active, switching..."
  gpudriver panfrost
  gpudriver --start
  echo "GPU driver after switch: $(gpudriver)"
else
  echo "panfrost is active, continuing..."
fi

# Log renderD128 availability
if [ -e /dev/dri/renderD128 ]; then
  echo "renderD128 is available"
else
  echo "WARNING: renderD128 not found!"
fi
systemctl stop essway
systemctl restart sway
sleep 1.5

RESOLUTION=$("sdl_resolution" 2>/dev/null | grep -a 'Current' | awk -F ': ' '{print $2}')

if [ -z "$RESOLUTION" ]; then
  DISPLAY_WIDTH=640
  DISPLAY_HEIGHT=480
else
  DISPLAY_WIDTH=$(echo "$RESOLUTION" | cut -d'x' -f 1)
  DISPLAY_HEIGHT=$(echo "$RESOLUTION" | cut -d'x' -f 2)
fi
usbgadget prepare
usbgadget network
echo "Launching mygame at ${DISPLAY_WIDTH}x${DISPLAY_HEIGHT}..."
mkdir -p /storage/mygame
/usr/bin/mygame --resolution ${DISPLAY_WIDTH}x${DISPLAY_HEIGHT} --main-pack /usr/share/mygame/mygame.pck

if [ -f "${GAMEDIR}/runtime/volume-indicator/teardown_vol_indicator" ]; then
  sh "${GAMEDIR}/runtime/volume-indicator/teardown_vol_indicator" "${SONGO_CFW_NAME}"
fi

# Generate a gamecontrollerdb for the launcher with xbox layout so physical
# A (East) → SDL BUTTON_A (confirm) and B (South) → SDL BUTTON_B (back).
# We source the ROCKNIX gamecontroller functions and call create_controller_db
# directly with ABUT/BBUT/XBUT/YBUT set for the xbox layout swap.
LAUNCHER_CTRLDB="/tmp/mygame-gamecontrollerdb.txt"
if [ -f /etc/profile.d/100-gamecontroller-functions ]; then
  ( . /etc/profile.d/100-gamecontroller-functions
    ABUT="b" BBUT="a" XBUT="y" YBUT="x"
    create_controller_db "xbox" "/tmp/es_input.cfg" "${LAUNCHER_CTRLDB}"
  ) 2>/dev/null || true
fi
if [ ! -s "${LAUNCHER_CTRLDB}" ] && [ -f "/storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt" ]; then
  cp "/storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt" "${LAUNCHER_CTRLDB}"
fi
[ -s "${LAUNCHER_CTRLDB}" ] && export SDL_GAMECONTROLLERCONFIG_FILE="${LAUNCHER_CTRLDB}"

launcher