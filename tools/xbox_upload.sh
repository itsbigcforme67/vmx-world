#!/bin/bash
# Installs VMX World's network loader on the real Xbox over FTP, once (copied from FOLD's): E:\Games\VMXWORLD\default.xbe
# (the loader, listed by Evolution X as VMX World) and loader.txt (this laptop's address, found automatically, and port
# 8771). From then on, starting VMX World fetches the newest build over HTTP from tools/xbox_serve.py (keep it running on the laptop) and
# starts it. Only the loader goes over FTP: Evolution X's FTP drops files bigger than about half a megabyte at
# random points (FOLD's 1.2 MB game failed four times in a row, 20 Oct 2026); the loader is 0.5 MB.
# Run by the owner, who types the FTP login (or has it in ~/.netrc as "machine <ip> login .. password .."); it is
# never stored. The console is found by trying the addresses it has had (192.168.1.115, .116); XBOX_IP=... to name it.
set -e
cd "$(dirname "$0")/.."
XBOX=${XBOX_IP:-}
if [ -z "$XBOX" ]; then
	for ip in 192.168.1.115 192.168.1.116; do
		if timeout 2 bash -c "</dev/tcp/$ip/21" 2>/dev/null; then XBOX=$ip; break; fi
	done
	[ -n "$XBOX" ] || { echo "no Xbox FTP server at 192.168.1.115 or .116: is it on (Evolution X running)? XBOX_IP=... to name it"; exit 1; }
fi
echo "Xbox at $XBOX"
tools/build_xbox.sh --loader >/dev/null
me=$(ip -4 route get "$XBOX" | sed -n 's/.* src \([0-9.]*\).*/\1/p')
[ -n "$me" ] || { echo "could not find this laptop's address towards $XBOX"; exit 1; }
echo "this laptop: $me:8771"
if grep -q "machine $XBOX" ~/.netrc 2>/dev/null; then
	AUTH=(--netrc)
else
	read -rp "Xbox FTP login: " u
	read -rsp "Xbox FTP password: " p; echo
	NETRC=$(mktemp); chmod 600 "$NETRC"
	printf 'machine %s login %s password %s\n' "$XBOX" "$u" "$p" > "$NETRC"
	AUTH=(--netrc-file "$NETRC")
fi
LT=$(mktemp); trap 'rm -f "$LT" ${NETRC:-}' EXIT
printf '%s:8771\n' "$me" > "$LT"
AUTH+=(--ftp-port -)     # Evolution X's FTP server answers 502 to EPSV / PASV: active mode only
URL=ftp://$XBOX/${XBOX_DIR:-E/Games/VMXWORLD}
put() {                  # a stall (15 s without progress) is given up and sent again, up to 4 tries
	for try in 1 2 3 4; do
		curl -sS "${AUTH[@]}" --ftp-create-dirs --speed-limit 1 --speed-time 15 -T "$1" "$URL/$2" && return 0
		echo "   stalled, again ($try)"; sleep 2
	done
	echo "   could not copy $2 (a half-copied file stays locked until the Xbox restarts: restart it and run this again)"
	return 1
}
echo "-> default.xbe (the loader)"; put xbox/build/xbox/loader/default.xbe default.xbe
echo "-> loader.txt"; put "$LT" loader.txt
echo "done. Now: tools/xbox_serve.py on this laptop, then on the Xbox: Play Games from Hard Drive -> VMX World"
