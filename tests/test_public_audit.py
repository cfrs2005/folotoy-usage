import base64
from pathlib import Path
import sys
import unittest
import tempfile
import subprocess
from unittest.mock import patch
import io
import contextlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import audit_public
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

    def test_default_audit_rejects_tracked_private_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            (root/'README.md').write_text('Public documentation')
            (root/'private.local.json').write_text('{}')
            subprocess.run(['git','init','--quiet',directory],check=True,capture_output=True)
            subprocess.run(['git','-C',directory,'add','.'],check=True,capture_output=True)
            with patch.object(audit_public,'ROOT',root), patch.object(audit_public,'FILES',['README.md']), patch.object(audit_public,'TREES',[]), patch.object(sys,'argv',['audit_public.py']), contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(audit_public.main(),1)
