#!/bin/bash
#
# Run indigo_server through launchd so macOS grants ImageCaptureCore access
# to PTP cameras.
#
# On recent macOS, ICA only delivers cameras to processes that (a) are signed
# with the com.apple.security.device.camera entitlement and (b) are their own
# TCC-responsible process. A server started from a terminal is attributed to
# the terminal app and silently sees no cameras; a launchd job qualifies.
#
# Usage:
#   indigo_tools/indigo_server_macos.sh [indigo_server arguments]
#   indigo_tools/indigo_server_macos.sh stop
#
# Example:
#   indigo_tools/indigo_server_macos.sh -vv indigo_ccd_ptp
#
# Logs are written to /tmp/indigo_server.log (override with INDIGO_SERVER_LOG)
# and tailed in the foreground; ^C stops the tail, not the server.

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVER="$ROOT/build/bin/indigo_server"
LOG="${INDIGO_SERVER_LOG:-/tmp/indigo_server.log}"
LABEL="indigo-server-dev"

if [ "$1" = "stop" ]; then
	launchctl remove "$LABEL" 2>/dev/null && echo "$LABEL stopped" || echo "$LABEL not running"
	exit 0
fi

if [ ! -x "$SERVER" ]; then
	echo "$SERVER not found - build it first (make)"
	exit 1
fi

if ! codesign -d --entitlements - "$SERVER" 2>/dev/null | grep -q com.apple.security.device.camera; then
	echo "warning: $SERVER lacks the camera entitlement, re-signing"
	codesign --force --sign - --entitlements "$ROOT/indigo_server/indigo_server.entitlements" "$SERVER" || exit 1
fi

launchctl remove "$LABEL" 2>/dev/null
rm -f "$LOG"
launchctl submit -l "$LABEL" -o "$LOG" -e "$LOG" -- "$SERVER" "$@"
echo "indigo_server running as launchd job '$LABEL', log: $LOG"
echo "stop with: $0 stop"
exec tail -F "$LOG"
