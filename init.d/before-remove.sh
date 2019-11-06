#!/bin/bash
# shim
# package pre-uninstallation script
# This script is run by the package management system before package
# is uninstalled.

if test -n "$(which systemctl 2>/dev/null)"; then
# SystemD
  systemctl -q stop shimvc 2>/dev/null || true
  systemctl -q disable shimvc 2>/dev/null || true
  rm -f /lib/systemd/system/shim.service
  systemctl -q daemon-reload 2>/dev/null || true
elif test -n "$(which update-rc.d 2>/dev/null)"; then
# Ubuntu
  service shimsvc stop
  update-rc.d -f shimsvc remove
  rm -f /etc/init.d/shimsvc
elif test -n "$(which chkconfig 2>/dev/null)"; then
# RHEL sysV
  service shimsvc stop
  chkconfig --del shimsvc
  chkconfig shimsvc off
  rm -f /etc/init.d/shimsvc
fi
