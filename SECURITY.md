# Security policy

Do not post secrets, provisioning JSON, screenshots containing account data,
serial logs or device dumps in public issues. Use GitHub private vulnerability
reporting when available; otherwise open an issue requesting a private contact
without disclosing vulnerability or credential details.

The firmware uses verified HTTPS and a read-only UsageHub Display Token. It does
not contain provider credentials, prompts or transcripts. Wi-Fi and the Display
Token are provisioned separately over a physically connected USB cable and kept
in NVS. NVS is not encrypted: physical flash access can recover credentials.
Revoke the Display Token before transferring hardware. Do not erase the entire
flash of a provisioned AI Passport: that also destroys its protected identity
and permanent recovery firmware.

Only upload firmware built from reviewed public source. Never upload a full
flash dump, NVS image, personal avatar, private endpoint, local configuration,
account export or an unreviewed screenshot. The source exporter uses an explicit
allowlist and scans content. Binary releases must pass the partition verifier
and public-content audit. These checks are defense in depth, not proof that an
arbitrary file is safe to disclose.
