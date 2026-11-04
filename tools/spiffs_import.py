#!/usr/bin/env python3
"""
Import MeshCore config from JSON directly to SPIFFS partition.
Works without CLI rescue mode - writes files directly to flash.
Usage: python3 tools/spiffs_import.py <config.json>
"""
import json
import struct
import os
import sys
import tempfile
import subprocess

SPIFFS_OFFSET = 0xC90000
SPIFFS_SIZE = 0x360000  # 3.456 MB
ESPPORT = '/dev/ttyACM0'

def run_cmd(cmd):
    print(f"  $ {cmd}")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  ERROR: {result.stderr}")
        return False
    if result.stdout.strip():
        print(f"  {result.stdout.strip()}")
    return True

def create_prefs_json(config):
    """Create prefs.json from config."""
    radio = config.get('radio_settings', {})
    pos = config.get('position_settings', {})
    other = config.get('other_settings', {})
    auto = config.get('auto_add_settings', {})
    
    # Build auto_add_config bitmask
    auto_mask = 0
    if auto.get('overwrite_oldest'): auto_mask |= 1
    if auto.get('auto_add_chat'): auto_mask |= 2
    if auto.get('auto_add_repeater'): auto_mask |= 4
    if auto.get('auto_add_room_server'): auto_mask |= 8
    if auto.get('auto_add_sensor'): auto_mask |= 16
    
    prefs = {
        "freq": radio.get('frequency', 868731) // 1000,  # stored in MHz
        "bw": radio.get('bandwidth', 62500) // 1000,    # stored in kHz
        "sf": radio.get('spreading_factor', 7),
        "cr": radio.get('coding_rate', 7),
        "tx_power_dbm": radio.get('tx_power', 10),
        "lat": float(pos.get('latitude', 0)),
        "lon": float(pos.get('longitude', 0)),
        "manual_add_contacts": other.get('manual_add_contacts', 1),
        "advert_location_policy": other.get('advert_location_policy', 0),
        "auto_add_config": auto_mask,
        "rx_boosted_gain": 1,
        "radio_fem_rxgain": 0,
        "mcu_sleep": 1,
        "ui_lang": 1,  # RU
        "tz_offset_mins": 180,  # UTC+3
        "time_valid": 1,
        "ble_pin": 123456,
        "wifi_enabled": 1,
        "wifi_ssid": "",
        "wifi_pwd": "",
        "ble_enabled": 1,
        "usb_enabled": 1,
        "clock_nodes": [],
        "telemetry_mode_base": 0,
        "telemetry_mode_loc": 0,
        "telemetry_mode_env": 0
    }
    return json.dumps(prefs, separators=(',', ':'))

def create_contacts_file(config):
    """Create contacts3 file (binary format)."""
    # MeshCore contacts format is binary, not JSON
    # For now, return empty - contacts need special handling
    return b''

def create_channels_file(config):
    """Create channels file."""
    channels = config.get('channels', [])
    # Channels also binary format
    return b''

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 spiffs_import.py <config.json>")
        sys.exit(1)
    
    with open(sys.argv[1]) as f:
        config = json.load(f)
    
    print("=== Creating SPIFFS image with config ===")
    
    # Create temp directory for SPIFFS files
    with tempfile.TemporaryDirectory() as tmpdir:
        # Write prefs.json
        prefs_json = create_prefs_json(config)
        prefs_path = os.path.join(tmpdir, 'prefs.json')
        with open(prefs_path, 'w') as f:
            f.write(prefs_json)
        print(f"Created prefs.json ({len(prefs_json)} bytes)")
        
        # Create empty placeholder files for contacts/channels
        # (they'll be populated via app after boot)
        for fname in ['contacts3', 'channels', 'adv_blobs', 'node_name']:
            with open(os.path.join(tmpdir, fname), 'wb') as f:
                f.write(b'')
        
        # Build SPIFFS image using mkspiffs
        spiffs_img = os.path.join(tmpdir, 'spiffs.img')
        cmd = f'mkspiffs -c {tmpdir} -b 4096 -p 256 -s {SPIFFS_SIZE} {spiffs_img}'
        if not run_cmd(cmd):
            print("mkspiffs not found. Install: apt install spiffs-tools")
            return False
        
        print(f"\n=== Flashing SPIFFS to 0x{SPIFFS_OFFSET:X} ===")
        flash_cmd = f'esptool --chip esp32s3 -p {ESPPORT} -b 460800 write-flash --flash-mode dio --flash-freq 40m --flash-size 16MB 0x{SPIFFS_OFFSET:X} {spiffs_img}'
        if not run_cmd(flash_cmd):
            return False
        
        print("\n=== Done! Reboot device to apply ===")
        return True

if __name__ == '__main__':
    main()