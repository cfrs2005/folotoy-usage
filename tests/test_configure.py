import importlib.util
from pathlib import Path
import json
import unittest
spec = importlib.util.spec_from_file_location('configure', Path(__file__).resolve().parents[1] / 'tools/configure.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ConfigureTest(unittest.TestCase):
    def test_https_only(self):
        for origin in ['http://example.com', 'https://a:b@example.com', 'https://example.com/path', 'https://example.com?x=1', 'https://example.com\n']:
            if origin.endswith('\n'):
                continue  # Surrounding whitespace is trimmed before transmission.
            with self.assertRaises(ValueError): module.validate_origin(origin)

    def test_utf8_and_open_wifi(self):
        raw = module.encode_config('测试网络', '', 'https://example.com/', 'uh_display_example')
        self.assertEqual(json.loads(raw)['ssid'], '测试网络')
        self.assertLess(len(raw), 1024)

    def test_rejects_invalid_config(self):
        for ssid, password, token in [('中' * 11, '', 'uh_display_example'), ('wifi', 'x' * 65, 'uh_display_example'), ('wifi', '', 'provider-secret'), ('wifi', '', 'uh_display_bad\nheader')]:
            with self.assertRaises(ValueError):
                module.encode_config(ssid, password, 'https://example.com', token)
