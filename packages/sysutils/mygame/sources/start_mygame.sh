#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

. /etc/profile

> "/storage/mygame/log.txt" && exec > >(tee "/storage/mygame/log.txt") 2>&1

# Check and log GPU driver
CURRENT_DRIVER=$(gpudriver)
echo "Current GPU driver: '${CURRENT_DRIVER}'"

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

systemctl restart sway
sleep 3

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
exec /usr/bin/mygame --resolution ${DISPLAY_WIDTH}x${DISPLAY_HEIGHT} --main-pack /usr/share/mygame/mygame.pck