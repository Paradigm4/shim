#!/bin/bash
# shim
# package pre-uninstallation script
# This script is run by the package management system before package
# is uninstalled.

if test -n "$(which systemctl 2>/dev/null)"; then
# SystemD
  systemctl -q stop shim 2>/dev/null || true
  systemctl -q disable  shim 2>/dev/null || true
  rm -f /usr/lib/systemd/system/shim.service
  systemctl -q daemon-reload 2>/dev/null || true
elif test -n "$(which update-rc.d 2>/dev/null)"; then
# Ubuntu
  update-rc.d -f shimsvc remove
  /etc/init.d/shimsvc stop
elif test -n "$(which chkconfig 2>/dev/null)"; then
# RHEL sysV
  chkconfig --del shimsvc
  chkconfig shimsvc off
  /etc/init.d/shimsvc stop
fi
