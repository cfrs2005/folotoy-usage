#!/usr/bin/env python3
"""Copy the paired workspace avatar to the display without embedding it in firmware."""
import argparse
import io
import json
import time
import urllib.request
from PIL import Image, ImageDraw, ImageOps
import serial
from configure import NoRedirect, validate_origin


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', required=True); p.add_argument('--port', required=True)
    args = p.parse_args()
    with open(args.config) as f: config = json.load(f)
    origin = validate_origin(config['origin'])
    headers = {'Authorization': 'Bearer ' + config['token']}
    opener = urllib.request.build_opener(NoRedirect)
    with opener.open(urllib.request.Request(origin+'/v1/display/profile',headers=headers),timeout=20) as response:
        profile = json.loads(response.read(4096))
    with opener.open(urllib.request.Request(origin+'/v1/display/avatar',headers=headers),timeout=20) as response:
        raw=response.read(2*1024*1024+1)
    if len(raw)>2*1024*1024: raise ValueError('Avatar too large')
    source=Image.open(io.BytesIO(raw))
    if source.width*source.height>4*1024*1024: raise ValueError('Avatar dimensions too large')
    source=ImageOps.fit(source.convert('RGB'),(40,40),method=Image.Resampling.LANCZOS)
    mask=Image.new('L',(40,40));ImageDraw.Draw(mask).ellipse((0,0,39,39),fill=255)
    image=Image.new('RGB',(40,40),(247,243,231));image.paste(source,(0,0),mask)
    pixels=bytearray()
    for r,g,b in zip(image.tobytes()[0::3],image.tobytes()[1::3],image.tobytes()[2::3]): pixels.extend((((r>>3)<<11)|((g>>2)<<5)|(b>>3)).to_bytes(2,'little'))
    payload=json.dumps({'avatar':pixels.hex(),'displayName':profile.get('displayName','UsageHub')[:16]},ensure_ascii=False).encode()+b'\n'
    with serial.Serial(args.port,115200,timeout=.2) as port:
        port.reset_input_buffer()
        for offset in range(0,len(payload),256):
            port.write(payload[offset:offset+256]);time.sleep(.005)
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            if b'USAGE_PROFILE_OK' in port.readline():
                print('Workspace avatar and name saved on device.');return
    raise RuntimeError('No profile confirmation')


if __name__=='__main__':main()
