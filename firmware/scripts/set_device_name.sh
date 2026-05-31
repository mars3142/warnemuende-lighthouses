#!/usr/bin/env bash
#
# Writes a device name into the NVS partition of a connected ESP32-H2.
# Required because the device uses USB Serial JTAG (no hardware UART),
# so runtime UART input during boot is not available.
#
# Usage:
#   ./scripts/set_device_name.sh <device-name> [port]
#   make set-name NAME="Leuchtturm West"
#   make set-name NAME="Leuchtturm West" PORT=/dev/cu.usbmodem1101
#
# The name is stored in NVS namespace "thread", key "name".
# It is broadcast in a CoAP multicast announcement (ff03::1/announce)
# when the device first attaches to the Thread network so the
# Master (ESP32-C6) can identify the device by name.
#
# Requires:
#   - IDF_PATH set (run: . $IDF_PATH/export.sh)
#   - esptool.py in PATH (provided by export.sh)
#   - Device connected via USB

set -euo pipefail

NAME="${1:-}"
PORT="${2:-}"

# ─── Usage ───────────────────────────────────────────────────────────────────

if [ -z "$NAME" ]; then
    echo "Usage: $0 <device-name> [port]"
    echo ""
    echo "  device-name  Human-readable name broadcast on Thread attach (CoAP announce)."
    echo "               Stored in NVS. Do not use commas in the name."
    echo "  port         Serial port, e.g. /dev/cu.usbmodem1101"
    echo "               Omit to auto-detect the first /dev/cu.usbmodem* device."
    echo ""
    echo "  make set-name NAME=\"Leuchtturm West\""
    echo "  make set-name NAME=\"Leuchtturm West\" PORT=/dev/cu.usbmodem1101"
    exit 1
fi

# ─── Sanity checks ───────────────────────────────────────────────────────────

if [ -z "${IDF_PATH:-}" ]; then
    echo "Error: IDF_PATH is not set."
    echo "Source the IDF environment first:"
    echo "  . \$IDF_PATH/export.sh"
    exit 1
fi

NVS_GEN="${IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"

if [ ! -f "$NVS_GEN" ]; then
    echo "Error: nvs_partition_gen.py not found: $NVS_GEN"
    exit 1
fi

if ! command -v esptool.py &>/dev/null; then
    echo "Error: esptool.py not found in PATH."
    echo "Source the IDF environment first:"
    echo "  . \$IDF_PATH/export.sh"
    exit 1
fi

# ─── Port detection ──────────────────────────────────────────────────────────

if [ -z "$PORT" ]; then
    PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
    if [ -z "$PORT" ]; then
        echo "Error: No USB Serial JTAG device found (/dev/cu.usbmodem*)."
        echo "Connect the ESP32-H2 via USB or specify PORT explicitly."
        exit 1
    fi
    echo "Port: $PORT (auto-detected)"
else
    echo "Port: $PORT"
fi

# ─── NVS partition parameters (must match partitions.csv) ────────────────────

NVS_OFFSET="0x9000"
NVS_SIZE="0x6000"   # 24 KiB

# ─── Generate NVS binary ─────────────────────────────────────────────────────

TMP_CSV=$(mktemp /tmp/nvs_name_XXXXXX.csv)
TMP_BIN=$(mktemp /tmp/nvs_name_XXXXXX.bin)
trap 'rm -f "$TMP_CSV" "$TMP_BIN"' EXIT

# CSV format expected by nvs_partition_gen.py
printf 'key,type,encoding,value\nthread,namespace,,\nname,data,string,%s\n' "$NAME" > "$TMP_CSV"

echo "Name:   \"$NAME\""
echo "Offset: $NVS_OFFSET  Size: $NVS_SIZE"
echo ""
echo "Generating NVS partition..."
python3 "$NVS_GEN" generate "$TMP_CSV" "$TMP_BIN" "$NVS_SIZE"

# ─── Flash ───────────────────────────────────────────────────────────────────

echo "Flashing to $PORT..."
esptool.py --chip esp32h2 --port "$PORT" write_flash "$NVS_OFFSET" "$TMP_BIN"

echo ""
echo "Done. Device name \"$NAME\" written to NVS."
echo "Power-cycle the ESP32-H2 to apply."
