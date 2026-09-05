#!/usr/bin/env python3
"""Capture actual device RGB565 render strips as a PNG (requires pyserial)."""
import argparse
import struct
import time
import zlib
from pathlib import Path
import serial


def png(width, height, rgb):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    raw = b''.join(b'\0' + rgb[y*width*3:(y+1)*width*3] for y in range(height))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', width, height, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', required=True); p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    rgb = bytearray(240*320*3); covered = bytearray(240*320)
    with serial.Serial(args.port, 115200, timeout=5) as port:
        port.reset_input_buffer(); port.write(b'{"screenshot":true}\n')
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            if port.readline().strip() == b'USAGE_SHOT 240 320': break
        else: raise RuntimeError('Screenshot header not received')
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            line = port.readline().strip()
            if line == b'END': break
            fields = line.split()
            if len(fields) != 6 or fields[0] != b'RECT': raise RuntimeError('Invalid frame header')
            x,y,w,h,n = map(int,fields[1:])
            if not (0<=x<x+w<=240 and 0<=y<y+h<=320 and n==w*h*2): raise RuntimeError('Invalid frame bounds')
            data = port.read(n)
            if len(data)!=n: raise RuntimeError(f'Truncated frame: expected {n}, received {len(data)}')
            for row in range(h):
                for col in range(w):
                    value = int.from_bytes(data[(row*w+col)*2:(row*w+col)*2+2], 'little')
                    i=(y+row)*240+x+col
                    rgb[i*3:i*3+3] = bytes((((value>>11)&31)*255//31,((value>>5)&63)*255//63,(value&31)*255//31))
                    covered[i]=1
        if not all(covered): raise RuntimeError('Incomplete screenshot')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(png(240,320,rgb))
    print(f'Actual device screenshot: {args.output}')


if __name__=='__main__': main()
