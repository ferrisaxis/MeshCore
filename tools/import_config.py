#!/usr/bin/env python3
"""
Import MeshCore config from JSON to device via USB serial.
Usage: python3 import_config.py <config.json>
"""
import json
import serial
import time
import sys

SERIAL_PORT = '/dev/ttyACM0'
BAUD = 115200

def send_cmd(ser, cmd, wait=0.5):
    """Send command and read response."""
    ser.write((cmd + '\n').encode())
    time.sleep(wait)
    response = ser.read_all().decode(errors='ignore')
    return response.strip()

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 import_config.py <config.json>")
        sys.exit(1)
    
    with open(sys.argv[1]) as f:
        config = json.load(f)
    
    print(f"Connecting to {SERIAL_PORT}...")
    ser = serial.Serial(SERIAL_PORT, BAUD, timeout=2)
    time.sleep(1)  # wait for connection to settle
    
    # Clear any pending data
    ser.read_all()
    
    print("=== Applying Radio Settings ===")
    radio = config.get('radio_settings', {})
    if radio:
        freq = radio.get('frequency', 868731)
        bw = radio.get('bandwidth', 62500)
        sf = radio.get('spreading_factor', 7)
        cr = radio.get('coding_rate', 7)
        tx = radio.get('tx_power', 10)
        
        # set radio.freq <khz>
        resp = send_cmd(ser, f'set radio.freq {freq}')
        print(f"radio.freq: {resp}")
        
        resp = send_cmd(ser, f'set radio.bw {bw}')
        print(f"radio.bw: {resp}")
        
        resp = send_cmd(ser, f'set radio.sf {sf}')
        print(f"radio.sf: {resp}")
        
        resp = send_cmd(ser, f'set radio.cr {cr}')
        print(f"radio.cr: {resp}")
        
        resp = send_cmd(ser, f'set radio.txpower {tx}')
        print(f"radio.txpower: {resp}")
    
    print("\n=== Applying Position ===")
    pos = config.get('position_settings', {})
    if pos:
        lat = pos.get('latitude', '0')
        lon = pos.get('longitude', '0')
        resp = send_cmd(ser, f'set position {lat} {lon}')
        print(f"position: {resp}")
    
    print("\n=== Applying Other Settings ===")
    other = config.get('other_settings', {})
    if other:
        manual = other.get('manual_add_contacts', 1)
        resp = send_cmd(ser, f'set manual_add_contacts {manual}')
        print(f"manual_add_contacts: {resp}")
        
        adv_policy = other.get('advert_location_policy', 0)
        resp = send_cmd(ser, f'set advert_location_policy {adv_policy}')
        print(f"advert_location_policy: {resp}")
    
    print("\n=== Applying Auto-Add Settings ===")
    auto = config.get('auto_add_settings', {})
    if auto:
        # Build bitmask
        mask = 0
        if auto.get('overwrite_oldest'): mask |= 1      # 0x01
        if auto.get('auto_add_chat'): mask |= 2         # 0x02
        if auto.get('auto_add_repeater'): mask |= 4     # 0x04
        if auto.get('auto_add_room_server'): mask |= 8  # 0x08
        if auto.get('auto_add_sensor'): mask |= 16      # 0x10
        # auto_add_max_hops not in bitmask, separate setting if exists
        
        resp = send_cmd(ser, f'set auto_add_config {mask}')
        print(f"auto_add_config: {resp} (mask={mask})")
    
    print("\n=== Applying Channels ===")
    channels = config.get('channels', [])
    for ch in channels:
        name = ch.get('name', '')
        secret = ch.get('secret', '')
        scope_name = ch.get('region_scope_name', '')
        scope_key = ch.get('region_scope_key', '')
        
        if name and secret:
            cmd = f'channel add "{name}" {secret}'
            if scope_name:
                cmd += f' "{scope_name}" {scope_key}'
            resp = send_cmd(ser, cmd, wait=1.0)
            print(f"channel add {name}: {resp}")
    
    print("\n=== Applying Contacts ===")
    contacts = config.get('contacts', [])
    for i, ct in enumerate(contacts):
        ctype = ct.get('type', 2)
        name = ct.get('name', '')
        pubkey = ct.get('public_key', '')
        lat = ct.get('latitude', '0')
        lon = ct.get('longitude', '0')
        
        if name and pubkey:
            # contact add <type> <name> <pubkey> [lat] [lon]
            cmd = f'contact add {ctype} "{name}" {pubkey} {lat} {lon}'
            resp = send_cmd(ser, cmd, wait=1.0)
            print(f"contact {i+1}/{len(contacts)} {name}: {resp[:80]}")
            if i % 10 == 0 and i > 0:
                time.sleep(0.5)  # throttle
    
    print("\n=== Saving and Rebooting ===")
    resp = send_cmd(ser, 'save', wait=1.0)
    print(f"save: {resp}")
    
    resp = send_cmd(ser, 'reboot', wait=2.0)
    print(f"reboot: {resp}")
    
    ser.close()
    print("\nDone!")

if __name__ == '__main__':
    main()