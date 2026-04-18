# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

PKG_NAME="mygame"
PKG_VERSION="1.0.0"
PKG_LICENSE="custom"
PKG_LONGDESC="My custom Godot game"
PKG_TOOLCHAIN="manual"
PKG_DEPENDS_TARGET="toolchain SDL2 SDL2_ttf"

make_target() {
  ${TARGET_CXX} ${TARGET_CXXFLAGS} \
    -std=c++17 \
    $(${SYSROOT_PREFIX}/usr/bin/sdl2-config --cflags 2>/dev/null || \
      echo "-I${SYSROOT_PREFIX}/usr/include/SDL2 -D_REENTRANT") \
    -o launcher \
    ${PKG_DIR}/launcher/launcher.cpp \
    $(${SYSROOT_PREFIX}/usr/bin/sdl2-config --libs 2>/dev/null || \
      echo "-L${SYSROOT_PREFIX}/usr/lib -lSDL2") \
    -lSDL2_ttf \
    ${TARGET_LDFLAGS}
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
  mkdir -p ${INSTALL}/usr/lib
  mkdir -p ${INSTALL}/usr/share/mygame

  # Copy game binary and data
  cp ${PKG_DIR}/sources/mygame ${INSTALL}/usr/bin/mygame
  chmod 0755 ${INSTALL}/usr/bin/mygame
  cp ${PKG_DIR}/sources/sdl_resolution ${INSTALL}/usr/bin/sdl_resolution
  chmod 0755 ${INSTALL}/usr/bin/sdl_resolution
  cp launcher ${INSTALL}/usr/bin/launcher
  chmod 0755 ${INSTALL}/usr/bin/launcher
  cp ${PKG_DIR}/sources/actions.sh ${INSTALL}/usr/bin/actions.sh
  chmod 0755 ${INSTALL}/usr/bin/actions.sh
  if [ -f ${PKG_DIR}/sources/mygame.pck ]; then
    cp ${PKG_DIR}/sources/mygame.pck ${INSTALL}/usr/share/mygame/
  fi

  cp ${PKG_DIR}/sources/libavcodec.so.62 ${INSTALL}/usr/lib/libavcodec.so.62
  cp ${PKG_DIR}/sources/libavformat.so.62 ${INSTALL}/usr/lib/libavformat.so.62
  cp ${PKG_DIR}/sources/libavutil.so.60 ${INSTALL}/usr/lib/libavutil.so.60
  cp ${PKG_DIR}/sources/libswresample.so.6 ${INSTALL}/usr/lib/libswresample.so.6
  cp ${PKG_DIR}/sources/start_mygame.sh ${INSTALL}/usr/bin/start_mygame.sh
  chmod 0755 ${INSTALL}/usr/bin/start_mygame.sh
}

post_install() {
  enable_service mygame.service
}
