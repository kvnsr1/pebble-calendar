#!/usr/bin/env python3
"""Add the documented Core 1.15 permission metadata to a native PBW."""
import json
import os
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parent.parent
pbw = root / 'build/pebble-calendar.pbw'
with zipfile.ZipFile(pbw) as archive:
    entries = [(entry, archive.read(entry.filename)) for entry in archive.infolist()]
info = json.loads(next(data for entry, data in entries if entry.filename == 'appinfo.json'))
info['manifestVersion'] = 2
info['usesPermissions'] = ['Calendar']
demo = os.environ.get('CALENDAR_DEMO') == '1'
if demo:
    info['shortName'] = info['longName'] = 'PebbleCal Demo'
temporary = pbw.with_suffix('.tmp')
with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED) as archive:
    for entry, data in entries:
        archive.writestr(entry, json.dumps(info).encode() if entry.filename == 'appinfo.json' else data)
temporary.replace(pbw)
destination = root / 'dist' / ('pebble-calendar-demo.pbw' if demo else 'pebble-calendar.pbw')
destination.parent.mkdir(exist_ok=True)
destination.write_bytes(pbw.read_bytes())
print(f'Packaged {destination.name} (Calendar permission only)')
