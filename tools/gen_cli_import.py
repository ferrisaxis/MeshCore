#!/usr/bin/env python3
"""
Generate full CLI import commands from JSON config.
Outputs to cli_import_full.txt with all contacts.
"""
import json
import sys

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 gen_cli_import.py <config.json>")
        sys.exit(1)
    
    with open(sys.argv[1]) as f:
        config = json.load(f)
    
    out_lines = []
    out_lines.append("# MeshCore Config Import Commands - FULL")
    out_lines.append("# Generated from JSON config")
    out_lines.append("# Run in CLI rescue mode (hold button during boot)")
    out_lines.append("")
    
    # Radio
    radio = config.get('radio_settings', {})
    out_lines.append("# ============================================================")
    out_lines.append("# RADIO SETTINGS")
    out_lines.append("# ============================================================")
    out_lines.append(f"set radio.freq {radio.get('frequency', 868731)}")
    out_lines.append(f"set radio.bw {radio.get('bandwidth', 62500)}")
    out_lines.append(f"set radio.sf {radio.get('spreading_factor', 7)}")
    out_lines.append(f"set radio.cr {radio.get('coding_rate', 7)}")
    out_lines.append(f"set radio.txpower {radio.get('tx_power', 10)}")
    out_lines.append("")
    
    # Position
    pos = config.get('position_settings', {})
    out_lines.append("# ============================================================")
    out_lines.append("# POSITION")
    out_lines.append("# ============================================================")
    out_lines.append(f"set position {pos.get('latitude', '0')} {pos.get('longitude', '0')}")
    out_lines.append("")
    
    # Other
    other = config.get('other_settings', {})
    out_lines.append("# ============================================================")
    out_lines.append("# OTHER SETTINGS")
    out_lines.append("# ============================================================")
    out_lines.append(f"set manual_add_contacts {other.get('manual_add_contacts', 1)}")
    out_lines.append(f"set advert_location_policy {other.get('advert_location_policy', 0)}")
    out_lines.append("")
    
    # Auto-add
    auto = config.get('auto_add_settings', {})
    mask = 0
    if auto.get('overwrite_oldest'): mask |= 1
    if auto.get('auto_add_chat'): mask |= 2
    if auto.get('auto_add_repeater'): mask |= 4
    if auto.get('auto_add_room_server'): mask |= 8
    if auto.get('auto_add_sensor'): mask |= 16
    out_lines.append("# ============================================================")
    out_lines.append("# AUTO-ADD CONFIG (bitmask)")
    out_lines.append(f"# mask = {mask}")
    out_lines.append("# ============================================================")
    out_lines.append(f"set auto_add_config {mask}")
    out_lines.append("")
    
    # Channels
    channels = config.get('channels', [])
    out_lines.append("# ============================================================")
    out_lines.append("# CHANNELS")
    out_lines.append("# ============================================================")
    for ch in channels:
        name = ch.get('name', '')
        secret = ch.get('secret', '')
        scope_name = ch.get('region_scope_name', '')
        scope_key = ch.get('region_scope_key', '')
        if name and secret:
            if scope_name:
                out_lines.append(f'channel add "{name}" {secret} "{scope_name}" {scope_key}')
            else:
                out_lines.append(f'channel add "{name}" {secret}')
    out_lines.append("")
    
    # Contacts
    contacts = config.get('contacts', [])
    out_lines.append("# ============================================================")
    out_lines.append(f"# CONTACTS ({len(contacts)} contacts)")
    out_lines.append("# ============================================================")
    out_lines.append("# Format: contact add <type> \"<name>\" <pubkey> <lat> <lon>")
    out_lines.append("")
    
    for ct in contacts:
        ctype = ct.get('type', 2)
        name = ct.get('name', '').replace('"', '\\"')
        pubkey = ct.get('public_key', '')
        lat = ct.get('latitude', '0')
        lon = ct.get('longitude', '0')
        if name and pubkey:
            out_lines.append(f'contact add {ctype} "{name}" {pubkey} {lat} {lon}')
    
    out_lines.append("")
    out_lines.append("# ============================================================")
    out_lines.append("# SAVE AND REBOOT")
    out_lines.append("# ============================================================")
    out_lines.append("save")
    out_lines.append("reboot")
    
    with open('/home/firid/MeshCore/tools/cli_import_full.txt', 'w') as f:
        f.write('\n'.join(out_lines))
    
    print(f"Generated {len(out_lines)} lines, {len(contacts)} contacts")
    print("Output: /home/firid/MeshCore/tools/cli_import_full.txt")

if __name__ == '__main__':
    main()