# Contributing

Use ESP-IDF 5.5.3 and run `./tools/validate.sh` before submitting a change. It runs
the public-content audit, host checks, firmware build and partition verification.
Use `--static` for the first two steps while iterating. Keep unrelated changes.

Keep board wiring and drivers in `components/bsp`, and application behavior in
`main`. Keep button callbacks non-blocking, hold the LVGL lock outside its task,
and budget for an ESP32-C3 without PSRAM. Preserve the 3 MB application limit,
identity at `0x356000`, Recovery at `0x700000` and the five-second UP boot gesture.

Report build, host tests and actual device checks separately. Device-rendered
screenshots may contain personal information; do not attach them publicly until
reviewed and anonymized. Do not change supported behavior merely to produce an
attractive mockup.

Explain the user-visible change and validation in pull requests. Contributions
use the repository MIT license; third-party fonts retain their OFL notices.
