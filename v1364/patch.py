#!/usr/bin/env python3
"""Reproducible, hash-pinned binary patch of the user-tested V13.6.3 SELF."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

INPUT_SHA = 'ae91dde5d75a33ca2db9ca0458acbd6c418526ff08780367d6baf42c9f8adbc1'
PKG_SHA = 'ba29d8ab812e5a5bda33c65d6d6c84e79e66634b81c54ed2b71e15f18433a5d5'
TEXT = 0x2740
CAVE = 0x3f0900
CAVE_END = 0x3f4000
HERE = Path(__file__).resolve().parent


def sha(b):
    return hashlib.sha256(b).hexdigest()


def build_hooks(build):
    build.mkdir(parents=True, exist_ok=True)
    subprocess.run(['cc', '-c', '-Os', '-std=c11', '-ffreestanding', '-fno-builtin',
                    '-fno-stack-protector', '-fPIC', '-fno-asynchronous-unwind-tables',
                    '-fno-unwind-tables', '-fno-tree-loop-distribute-patterns',
                    '-Wall', '-Wextra', '-Werror', str(HERE/'hooks.c'), '-o', str(build/'c.o')], check=True)
    subprocess.run(['cc', '-c', str(HERE/'hooks.S'), '-o', str(build/'asm.o')], check=True)
    subprocess.run(['ld', '-T', str(HERE/'hooks.ld'), '-o', str(build/'hooks.elf'),
                    str(build/'asm.o'), str(build/'c.o')], check=True)
    subprocess.run(['objcopy', '-O', 'binary', '--only-section=.payload',
                    str(build/'hooks.elf'), str(build/'hooks.bin')], check=True)
    symbols = {}
    for line in subprocess.check_output(['nm', '-n', str(build/'hooks.elf')], text=True).splitlines():
        address, kind, name = line.split()
        if kind.upper() == 'U':
            raise ValueError('unresolved import: '+name)
        symbols[name] = int(address, 16)
    return (build/'hooks.bin').read_bytes(), symbols


def patch(src, payload, symbols):
    if sha(src) != INPUT_SHA:
        raise ValueError('Wrong input SELF; refusing offsets from a different build: '+sha(src))
    if len(payload) > CAVE_END-CAVE or any(src[TEXT+CAVE:TEXT+CAVE_END]):
        raise ValueError('Payload exceeds verified empty RX padding')
    out = bytearray(src)
    changes = []

    def replace(va, expected, new, reason):
        assert len(expected) == len(new)
        assert src[TEXT+va:TEXT+va+len(expected)] == expected, hex(va)
        out[TEXT+va:TEXT+va+len(new)] = new
        changes.append(dict(va=va, size=len(new), reason=reason,
                            before=expected.hex(), after=new.hex()))

    def branch(va, expected_hex, target, opcode=0xe8):
        old = bytes.fromhex(expected_hex)
        new = bytes([opcode])+struct.pack('<i', symbols[target]-(va+5))
        replace(va, old, new+b'\x90'*(len(old)-5), target)

    branch(0x21bca, 'e831120000', 'scan_entry')
    branch(0x21c66, 'e8b5ecffff', 'native_entry')
    branch(0x22a72, 'e8a9deffff', 'native_entry')
    branch(0x2225d, 'e87e120000', 'mount_preflight')
    branch(0x22338, 'e8c3120000', 'decrypt_logged')
    branch(0x22e24, '488b05a5123d00', 'fill_fourth_root', opcode=0xe9)
    replace(0x22e5c, b'\x18', b'\x20', 'three original roots plus external metadata')
    branch(0x2266f, 'ffd0898578f9ffff', 'mount_first')
    branch(0x22709, 'ffd0898564f9ffff', 'mount_second')
    branch(0x229c7, 'ffd0898520f9ffff', 'unmount_entry')
    branch(0x23726, 'e89f1c0000', 'ioctl_padded')
    for old, new in [(b'V13.6 - TROFEU SEM JOGO / TROPHY.IMG', b'V13.6.4 TEST - TROFEUS SEM JOGO'),
                     (b'Trophy Unlocker V13.6', b'Trophy Unlock V13.6.4')]:
        assert src.count(old+b'\0') == 1
        at = src.index(old+b'\0')-TEXT
        replace(at, old+b'\0', new+b'\0'*(len(old)+1-len(new)), 'version label')
    replace(CAVE, bytes(len(payload)), payload, 'position-independent payload')

    # Protect all bytes outside the explicit patch list, including SELF tables,
    # imports, runtime data, the entire installed/common unlock routine and UI.
    allowed = bytearray(len(src))
    for change in changes:
        start = TEXT+change['va']
        allowed[start:start+change['size']] = b'\1'*change['size']
    assert all(a == b or allowed[i] for i, (a, b) in enumerate(zip(src, out)))
    assert len(src) == len(out)
    assert src[TEXT+0x20920:TEXT+0x21a20] == out[TEXT+0x20920:TEXT+0x21a20]
    return bytes(out), dict(input_sha256=sha(src), output_sha256=sha(out),
                           installed_unlock_unchanged=True, hardware_tested=False,
                           symbols=symbols, changes=changes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('input', type=Path)
    ap.add_argument('output', type=Path)
    ap.add_argument('--build-dir', type=Path, required=True)
    args = ap.parse_args()
    payload, symbols = build_hooks(args.build_dir)
    out, report = patch(args.input.read_bytes(), payload, symbols)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(out)
    (args.build_dir/'patch-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Patched SELF SHA256:', sha(out))
    print('Installed unlock code unchanged. PS4 hardware validation pending.')


if __name__ == '__main__':
    main()
