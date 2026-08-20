import importlib.util
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("xovigen", ROOT / "util" / "xovigen.py")
XOVIGEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(XOVIGEN)


class CStringLiteralTests(unittest.TestCase):
    def test_nul_before_octal_digit_uses_fixed_width_escape(self):
        self.assertEqual(XOVIGEN.c_string_literal(b"key\0" + b"3.27"), '"key\\0003.27"')

    def test_literal_round_trips_every_byte_through_c_compiler(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("a host C compiler is not available")

        value = bytes(range(256))
        expected = ", ".join(str(byte) for byte in value)
        source = f"""
#include <string.h>
static const unsigned char actual[] = {XOVIGEN.c_string_literal(value)};
static const unsigned char expected[] = {{ {expected} }};
_Static_assert(sizeof(actual) == sizeof(expected) + 1, "literal length changed");
int main(void) {{ return memcmp(actual, expected, sizeof(expected)) != 0; }}
"""
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source_path = directory / "literal.c"
            executable_path = directory / "literal"
            source_path.write_text(source, encoding="ascii")
            subprocess.run(
                [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", source_path, "-o", executable_path],
                check=True,
            )
            subprocess.run([executable_path], check=True)


class MetadataTableTests(unittest.TestCase):
    def test_offsets_are_utf8_byte_offsets_and_survive_numeric_values(self):
        header = XOVIGEN.HeaderState()
        owner = XOVIGEN.HeaderAddition(XOVIGEN.HeaderAdditionType.Export, "handler")

        header.add_metadata_entry_for_entry(
            XOVIGEN.GLOBAL_METADATA,
            "显示名",
            XOVIGEN.MetadataType.String,
            "测试",
        )
        header.add_metadata_entry_for_entry(
            XOVIGEN.GLOBAL_METADATA,
            "xochitlVersions",
            XOVIGEN.MetadataType.String,
            "3.27.x.x",
        )
        header.add_metadata_entry_for_entry(
            owner,
            "xovi-message-broker$simpleSignal",
            XOVIGEN.MetadataType.String,
            "demo$capabilities",
        )

        expected = (
            "显示名\0测试\0"
            "xochitlVersions\0"
            "3.27.x.x\0"
            "xovi-message-broker$simpleSignal\0demo$capabilities\0"
        ).encode("utf-8")
        self.assertEqual(bytes(header.metadata_name_table), expected)

        for entries in header.metadata.values():
            for entry in entries:
                name_end = header.metadata_name_table.index(0, entry.name_offset)
                self.assertGreater(name_end, entry.name_offset)
                if entry.type_ is XOVIGEN.MetadataType.String:
                    length, offset = entry.value
                    self.assertEqual(len(header.metadata_name_table[offset:offset + length]), length)
                    self.assertEqual(header.metadata_name_table[offset + length], 0)


if __name__ == "__main__":
    unittest.main()
