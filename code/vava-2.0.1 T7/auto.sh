#!/bin/sh
export STAGING_DIR=/home/openwrt/OpenWrt-SDK-ramips-mt7688_gcc-4.8-linaro_uClibc-0.9.33.2.Linux-x86_64/staging_dir
make clean
make
mipsel-openwrt-linux-strip Ppcs_vava
cp ./Ppcs_vava /home/nfs/
sync
