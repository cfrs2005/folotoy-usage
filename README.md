[简体中文](README.zh_CN.md)

# FoloToy Usage

[![Build](https://github.com/cfrs2005/folotoy-usage/actions/workflows/build.yml/badge.svg)](https://github.com/cfrs2005/folotoy-usage/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

[Download firmware](https://github.com/cfrs2005/folotoy-usage/releases/latest) · [中文说明](README.zh_CN.md)

An open-source UsageHub display for the FoloToy AI Passport: ESP32-C3,
240 x 320 display, 8 MB flash, no PSRAM. This is a standalone firmware project.
It builds without the adjacent story appliance or any private cloud source.

- Paper background, red Claude, blue Codex, Chinese labels and a personal profile.
- Home shows your avatar, name, local date/time and both providers with large
  quota percentages, progress bars and reset countdowns.
- The second page shows both providers with today/cumulative tokens and costs.
  Dedicated IBM Plex Mono numerals and Noto Sans SC labels are rendered natively
  for the 240 x 320 display.
- Home selects the same 5-hour and 7-day quota buckets as the web console.
- UP/DOWN switch the two pages. OK on Home switches used/remaining. Hold OK to
  refresh (OK on the consumption page also refreshes). There is no bottom menu.
  Automatic refresh runs every minute; reset countdowns update on-device.
  Time defaults to UTC; a private USB `timezone` command stores a POSIX TZ string.
  Daylight-saving rules are applied by the device.
- Missing values show `--`. Network errors retain the last in-memory snapshot.
  Snapshots older than three minutes or marked stale by the service show an
  aged-data label. Quota and token freshness are handled separately.
  API-equivalent cost is not a subscription bill.
- No microphone, TTS, model credentials, collector or cloud implementation.

## Build and install

Install and activate ESP-IDF **5.5.3**, then run from this directory:

```sh
./tools/validate.sh
idf.py -p /dev/cu.YOUR_DEVICE flash
python tools/configure.py --port /dev/cu.YOUR_DEVICE
```

Configuration prompts for 2.4 GHz Wi-Fi and a UsageHub read-only Display Token
or single-use **display** enrollment code. Sign in to [UsageHub](https://u.80aj.com)
to create one. Collector enrollment codes are not supported. The host exchanges
a display enrollment code over verified HTTPS; only the read-only token reaches
the board. Close serial monitors before configuring. ESP-IDF includes pyserial.

A custom compatible service can be selected with `--origin https://example.com`.
The device calls `GET /v1/dashboard` with Bearer authentication and verifies TLS
using the certificate bundle after network time synchronization. Redirects are
rejected. Wi-Fi without Internet/NTP cannot refresh data.

Credentials are stored in the board's NVS, not embedded into source or shared
firmware. NVS is not encrypted: physical flash access can recover credentials.
Revoke the Display Token and clear the `usage` NVS namespace before transferring
hardware. Do not upload flash dumps or a configured device's NVS image.


Wi-Fi can be saved before pairing: `python tools/configure.py --port PORT --wifi-only`.
Use `--wifi-file wifi.local.json` to read a private local file containing `ssid`
and `password`. Existing device pairing is preserved. The unpaired device joins
Wi-Fi and asks for authorization on screen.

## Firmware compatibility

The full artifact is `build/FoloToy-AI-Passport-full.bin`; the checker validates
image offsets, partition MD5, the 3 MB application limit, protected identity at
`0x356000`, Recovery at `0x700000`, and the five-second UP boot gesture.
Use segmented `idf.py flash` on an existing device. Never use `erase-flash`.
Mini-program install compatibility is structurally checked. USB installation,
cloud loading, profile sync, screen rendering and repeated page/lens changes
have been checked on a device. Mini-program installation and extended unattended
stability are not yet verified. The story audio resource
is not included or used by this firmware.

## Open-source boundary

Publish **only this directory**, or the source ZIP produced by
`python3 tools/package_source.py`. The parent contains private backups and
configured story releases. Do not publish it as a whole.

Public: firmware, board drivers, font subset, USB provisioning tool, tests,
build workflow and this documentation. Private: SaaS, account data, provisioning
values, local build directories and device dumps. Releases contain generic
firmware, never an image read back from a configured device.

The existing open collectors are maintained in
[UsageHub Open](https://github.com/cfrs2005/usagehub-open). Use its onboarding
instructions to populate the same workspace displayed by this device.

## Development and licensing

`./tools/validate.sh --static` runs host checks; `--firmware` builds and verifies
images. Tests cover absent vs zero values, large token totals, invalid payloads,
duplicate providers, configuration input and protected firmware partitions.

Board code and recovery hook are derived from FoloToy AI Passport under MIT;
see [LICENSE](LICENSE). New firmware code uses the same license. The generated
Noto Sans SC font subset uses SIL OFL 1.1; see
[font license](assets/fonts/OFL.txt). Dependencies retain their own licenses.
The source package does not bundle ESP-IDF or downloaded components.

## Actual screen capture and profile sync

`python tools/screenshot.py --port PORT --output screen.png` captures the actual
LVGL render strips from the board over USB. It does not use a mockup or allocate
a full framebuffer on the device. Only run it with an attached reader; each
USB write has a bounded timeout. The screenshot may contain private usage data.

`python tools/sync_profile.py --port PORT --config device.local.json` reads the
paired cloud profile, fits its avatar to 40 pixels and stores it separately in
NVS. The private JSON contains `origin` and the read-only `token`. Install Pillow
and pyserial in a host virtual environment for this optional tool. No personal
avatar or token is compiled into firmware. The bundled font supports ASCII and
the shipped Chinese labels; arbitrary Chinese profile names may need more glyphs.

The usage page abbreviates token counts with K/M/B/T/P. Costs below $1,000 show
cents; larger costs show rounded dollars (or millions). These are display
roundings only; the cloud retains the full values.

## Instrument design

The [design brief and working prompts](docs/design-prompt.md) explain the research,
information hierarchy and hardware decisions. The home page uses two parallel
quota columns per provider, segmented meters and clear aged-data labels. The
public archive contains both font licenses and no personal profile data.

IBM Plex Mono numerals also use [SIL OFL 1.1](assets/fonts/IBM-Plex-OFL.txt).
The USB command `{"diagnostics":true}` reports only page/lens state, data presence
and free memory. It never returns Wi-Fi settings, account data or tokens.

## Contributing and security

See [CONTRIBUTING.md](CONTRIBUTING.md) for local validation and
[SECURITY.md](SECURITY.md) for credential boundaries and private reports.
Run `python3 tools/audit_public.py` before sharing an archive or pushing changes.
