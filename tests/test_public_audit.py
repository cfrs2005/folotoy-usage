import base64
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from audit_public import findings


class PublicAuditTest(unittest.TestCase):
    def test_pem_parser_marker_is_not_key_material(self):
        marker=b'-----BEGIN '+b'PRIVATE KEY-----'
        self.assertNotIn('private key',findings(marker+b'\0',[]))
        self.assertIn('private key',findings(marker+b'\n'+b'A'*96,[]))

    def test_long_display_credential_is_rejected(self):
        self.assertIn('UsageHub credential',findings(b'uh_'+b'display_'+b'x'*32,[]))
        self.assertNotIn('UsageHub credential',findings(b'uh_'+b'display_example',[]))

    def test_known_sensitive_value_encodings(self):
        value='private-test-secret'
        for raw in [value.encode(),base64.b64encode(value.encode()),value.encode().hex().encode()]:
            self.assertIn('private release value',findings(raw,[value]))
