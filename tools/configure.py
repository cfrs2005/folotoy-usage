#!/usr/bin/env python3
"""Configure a Usage display over USB without putting secrets in shell history."""
import argparse
import getpass
import json
import time
import urllib.parse
import urllib.request
import uuid
from pathlib import Path


def validate_origin(value):
    value = value.strip().rstrip('/')
    p = urllib.parse.urlsplit(value)
    if p.scheme != 'https' or not p.hostname or p.username or p.password or p.path or p.query or p.fragment:
        raise ValueError('Use an HTTPS origin without a path or credentials')
    if len(value.encode()) > 180 or any(ord(c) < 33 for c in value):
        raise ValueError('Invalid origin')
    return value


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def resolve_token(origin, token):
    if token.startswith('uh_display_') and 11 < len(token) <= 256:
        return token
    if not token.startswith('uh_enroll_') or len(token) > 256:
        raise ValueError('Use a Display Token or a display enrollment code')
    body = json.dumps({'token': token, 'deviceId': f'folotoy-{uuid.uuid4()}'}).encode()
    req = urllib.request.Request(origin + '/v1/device-enrollments/redeem', data=body,
                                 headers={'Content-Type': 'application/json'}, method='POST')
    with urllib.request.build_opener(NoRedirect).open(req, timeout=20) as result:
        data = result.read(4097)
    if len(data) > 4096:
        raise ValueError('Pairing response too large')
    value = json.loads(data).get('token', '')
    if not value.startswith('uh_display_') or not 11 < len(value) <= 256:
        raise ValueError('Pairing did not return a Display Token')
    return value


def encode_config(ssid, password, origin, token):
    config = dict(ssid=ssid, password=password, origin=validate_origin(origin), token=token)
    for name, limit in [('ssid', 32), ('password', 64), ('origin', 180), ('token', 256)]:
        value = config[name]
        if len(value.encode()) > limit or any(ord(c) < 32 or ord(c) == 127 for c in value):
            raise ValueError(f'Invalid {name}')
        if name != 'password' and not value:
            raise ValueError(f'Missing {name}')
    if not token.startswith('uh_display_') or len(token) <= 11:
        raise ValueError('Invalid Display Token')
    return json.dumps(config, ensure_ascii=False, separators=(',', ':')).encode() + b'\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='USB serial port')
    parser.add_argument('--origin', default='https://u.80aj.com')
    parser.add_argument('--wifi-only', action='store_true', help='Save Wi-Fi now and pair UsageHub later')
    parser.add_argument('--wifi-file', type=Path, help='Private JSON file with ssid and password')
    args = parser.parse_args()
    import serial  # Included in the activated ESP-IDF Python environment.
    origin = validate_origin(args.origin)
    if args.wifi_file:
        wifi = json.loads(args.wifi_file.read_text())
        ssid, password = wifi['ssid'], wifi['password']
    else:
        ssid = input('Wi-Fi SSID (2.4 GHz): ')
        password = getpass.getpass('Wi-Fi password (empty for open network): ')
    # Validate Wi-Fi before consuming a one-use enrollment code.
    encode_config(ssid, password, origin, 'uh_display_placeholder')
    raw_token = '' if args.wifi_only else getpass.getpass('Display Token or display enrollment code: ').strip()
    with serial.Serial(args.port, 115200, timeout=0.2, write_timeout=5) as port:
        port.reset_input_buffer()
        if args.wifi_only:
            # Omit cloud fields so an existing pairing is preserved on the device.
            payload = json.dumps({'ssid': ssid, 'password': password}, ensure_ascii=False).encode() + b'\n'
        else:
            token = resolve_token(origin, raw_token)
            payload = encode_config(ssid, password, origin, token)
        port.write(payload)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            line = port.readline()
            if b'USAGE_CONFIG_OK' in line:
                print('Configuration saved. The display is restarting.')
                return
            if b'USAGE_CONFIG_ERROR' in line:
                raise RuntimeError('The device rejected the configuration')
        raise RuntimeError('No confirmation received; check USB and retry with a new pairing code')


if __name__ == '__main__':
    try:
        main()
    except (Exception, KeyboardInterrupt) as error:
        # Never echo server bodies, serial data, tokens, or exception arguments.
        print(f'Configuration failed ({type(error).__name__}). Check connection and input.')
        raise SystemExit(1)
