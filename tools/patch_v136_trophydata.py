#!/usr/bin/env python3
"""Minimal patch for the shipped V13.6 eboot.

Keeps the V13.6 UI/library/trophy discovery byte-for-byte and changes only the
four dlsym targets used by the uninstalled-game trophy.img mounting path:
SaveData API -> TrophyData API.

Input is the eboot.bin extracted from
Trophy_Unlocker_13.52_V13_6_Trofeu_Sem_Jogo.pkg.
"""
from pathlib import Path
import hashlib
import struct
import sys

EXPECTED_IN = "88700ccf6b5a10071e4cc7962ad57550d264dc1da909c0124ffee5ab6780df02"
EXPECTED_OUT = "ea519d9c459f221d2990b7398f495b0f20ac7597b932e02991f25391f5c78a46"
SELF_TEXT_DATA_OFFSET = 0x2740
INJECT_VA = 0x3F082E

PATCHES = [
    (0x23556, b"sceFsInitMountTrophyDataOpt\0"),
    (0x2357C, b"sceFsMountTrophyData\0"),
    (0x235A2, b"sceFsInitUmountTrophyDataOpt\0"),
    (0x235C8, b"sceFsUmountTrophyData\0"),
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} input-eboot.bin output-eboot.bin")
        return 2
    src = Path(sys.argv[1]).read_bytes()
    if sha(src) != EXPECTED_IN:
        raise SystemExit(f"wrong V13.6 eboot: sha256={sha(src)}")
    out = bytearray(src)
    cursor = INJECT_VA
    for insn_va, symbol in PATCHES:
        symbol_va = cursor
        raw_symbol = SELF_TEXT_DATA_OFFSET + symbol_va
        if any(out[raw_symbol:raw_symbol + len(symbol)]):
            raise SystemExit(f"injection area not empty at 0x{symbol_va:x}")
        out[raw_symbol:raw_symbol + len(symbol)] = symbol

        raw_insn = SELF_TEXT_DATA_OFFSET + insn_va
        if out[raw_insn:raw_insn + 3] != b"\x48\x8d\x35":
            raise SystemExit(f"unexpected LEA at 0x{insn_va:x}")
        disp = symbol_va - (insn_va + 7)
        struct.pack_into("<i", out, raw_insn + 3, disp)
        cursor += len(symbol)

    result = bytes(out)
    if sha(result) != EXPECTED_OUT:
        raise SystemExit(f"unexpected output sha256={sha(result)}")
    Path(sys.argv[2]).write_bytes(result)
    print(EXPECTED_OUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
