#!/usr/bin/env python3
"""Execute actual linked x86-64 hooks under Unicorn with mocked PS4 APIs.

Checks ABI/ASLR/failure paths/stack bounds, not firmware or trophy compatibility.
Usage: python test_hooks.py build-dir original-eboot.bin
Dependency: unicorn==2.1.4
"""
import json
from pathlib import Path
import struct
import sys
import unittest
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *
from patch import patch, TEXT, CAVE

BUILD, INPUT = map(Path, sys.argv[1:3])
sys.argv[1:3] = []
REPORT = json.loads((BUILD/'patch-report.json').read_text())
SYMS = REPORT['symbols']
SRC = INPUT.read_bytes()
PATCHED, _ = patch(SRC, (BUILD/'hooks.bin').read_bytes(), SYMS)
REG_ARGS = [UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RDX,
            UC_X86_REG_RCX, UC_X86_REG_R8, UC_X86_REG_R9]
SAVED = [UC_X86_REG_RBX, UC_X86_REG_RBP, UC_X86_REG_R12,
         UC_X86_REG_R13, UC_X86_REG_R14, UC_X86_REG_R15]


class VM:
    def __init__(self, base=0, failures=None):
        self.base = base
        self.u = Uc(UC_ARCH_X86, UC_MODE_64)
        self.u.mem_map(base, 0x500000)
        self.u.mem_write(base, PATCHED[TEXT:TEXT+0x3f4000])
        self.u.mem_map(base+0x800000, 0x20000)
        self.stop = base+0x800000
        self.scratch = base+0x801000
        self.errno = base+0x80f000
        self.u.mem_map(base+0x1000000, 0x10000)
        self.sp = base+0x100e008
        self.fp = base+0x100f000
        self.calls = []
        self.lines = []
        self.failures = failures or {}
        self.mock = {}
        self.api = {base+v: k for k, v in SYMS.items()
                    if k.startswith('ps_') or k in ['tu_errno', 'tu_ioctl',
                       'original_load_api', 'original_decrypt', 'original_orphan_scan',
                       'original_native_unlock']}
        self.u.hook_add(UC_HOOK_CODE, self.hook)
        self.put32(self.errno, 0x55)
        for name in ['mount_init_ptr', 'mount_ptr', 'unmount_init_ptr', 'unmount_ptr']:
            self.put64(base+SYMS[name], 0)

    def put64(self, p, n): self.u.mem_write(p, struct.pack('<Q', n & ((1<<64)-1)))
    def get64(self, p): return struct.unpack('<Q', self.u.mem_read(p, 8))[0]
    def put32(self, p, n): self.u.mem_write(p, struct.pack('<I', n & 0xffffffff))
    def cstr(self, p): return bytes(self.u.mem_read(p, 160)).split(b'\0', 1)[0]

    def hook(self, u, pc, size, _):
        if pc == self.stop:
            u.emu_stop()
            return
        name = self.api.get(pc)
        if pc in self.mock:
            name, handler = self.mock[pc]
        elif name:
            handler = None
        else:
            return
        sp = u.reg_read(UC_X86_REG_RSP)
        assert sp % 16 == 8, (name, 'bad SysV stack alignment', hex(sp))
        args = [u.reg_read(r) for r in REG_ARGS]
        self.calls.append((name, args, sp))
        if handler:
            result = handler(args)
        elif name == 'tu_errno': result = self.errno
        elif name == 'ps_open': result = self.failures.get('open', 42)
        elif name == 'ps_write':
            count = min(args[2], self.failures.get('write_limit', args[2]))
            self.lines.append(bytes(u.mem_read(args[1], count)))
            result = count
        elif name == 'ps_load':
            assert self.cstr(args[0]) == b'/system/common/lib/libkernel_sys.sprx'
            assert args[1:] == [0]*5
            result = self.failures.get('load', 17)
        elif name == 'ps_dlsym':
            assert args[0] == 17 and self.cstr(args[1]) == b'statfs'
            self.put64(args[2], 0 if self.failures.get('null_statfs') else self.base+0x51000)
            result = self.failures.get('dlsym', 0)
        elif name == 'original_load_api':
            for symbol in ['mount_init_ptr', 'mount_ptr', 'unmount_init_ptr', 'unmount_ptr']:
                self.put64(self.base+SYMS[symbol], self.base+0x50000)
            if self.failures.get('null_api'):
                self.put64(self.base+SYMS['mount_ptr'], 0)
            result = self.failures.get('api', 0)
        elif name == 'original_orphan_scan': result = 1
        elif name == 'original_native_unlock': result = args[0]
        elif name == 'original_decrypt': result = self.failures.get('decrypt', -123)
        elif name == 'tu_ioctl': raise AssertionError('ioctl needs an explicit payload mock')
        else: result = 0
        # Stress ABI preservation: a real callee may overwrite all volatile regs.
        for r in REG_ARGS + [UC_X86_REG_R10, UC_X86_REG_R11]: u.reg_write(r, 0xcafe)
        u.reg_write(UC_X86_REG_RAX, result & ((1<<64)-1))
        u.reg_write(UC_X86_REG_RIP, self.get64(sp))
        u.reg_write(UC_X86_REG_RSP, sp+8)

    def run(self, name, args=(), rax=0):
        for i, r in enumerate(SAVED): self.u.reg_write(r, 0x123000+i)
        self.u.reg_write(UC_X86_REG_RBP, self.fp)
        self.u.reg_write(UC_X86_REG_RSP, self.sp)
        self.u.reg_write(UC_X86_REG_RAX, rax)
        for r, n in zip(REG_ARGS, args): self.u.reg_write(r, n)
        self.put64(self.sp, self.stop)
        saved = [self.u.reg_read(r) for r in SAVED]
        self.u.emu_start(self.base+SYMS[name], self.stop+1, count=200000)
        assert self.u.reg_read(UC_X86_REG_RIP) == self.stop, 'did not return'
        assert self.u.reg_read(UC_X86_REG_RSP) == self.sp+8, 'unbalanced stack'
        assert [self.u.reg_read(r) for r in SAVED] == saved, 'callee-saved register changed'
        assert struct.unpack('<I', self.u.mem_read(self.errno, 4))[0] == 0x55
        return self.u.reg_read(UC_X86_REG_EAX)


class Hooks(unittest.TestCase):
    def test_hash_guard_and_installed_code(self):
        self.assertEqual(SRC[TEXT+0x20920:TEXT+0x21a20], PATCHED[TEXT+0x20920:TEXT+0x21a20])
        with self.assertRaises(ValueError): patch(SRC[:-1]+b'x', b'', SYMS)

    def test_tail_trampolines_preserve_cpp_callers(self):
        for base in [0, 0x500000000]:
            for entry, dest, n in [('scan_entry', 'original_orphan_scan', 4),
                                    ('native_entry', 'original_native_unlock', 6)]:
                v = VM(base)
                args = [100+i for i in range(n)]
                v.run(entry, args)
                call = next(c for c in v.calls if c[0] == dest)
                self.assertEqual(call[1][:n], args)
                self.assertEqual(call[2], v.sp)  # no extra C++ unwind frame
                if entry == 'scan_entry':
                    op = next(c for c in v.calls if c[0] == 'ps_open')
                    self.assertEqual(op[1][1], 0x601)

    def test_fourth_root_and_original_array(self):
        for base in [0, 0x500000000]:
            v = VM(base)
            # Run original initialization through the actual patched jump.
            for i in range(3): v.put64(base+0x3f40d0+8*i, base+0x810000+i*32)
            v.u.reg_write(UC_X86_REG_RBP, v.fp)
            v.u.reg_write(UC_X86_REG_RSP, v.sp)
            v.u.mem_write(v.fp-0x50, b'\xa5'*0x28)
            v.u.emu_start(base+0x22e24, base+0x22e61, count=100)
            roots = [v.get64(v.fp-0x50+8*i) for i in range(4)]
            self.assertEqual(roots[:3], [base+0x810000+i*32 for i in range(3)])
            self.assertEqual(v.cstr(roots[3]), b'/data/TrophyUnlocker1352/metadata')
            self.assertEqual(v.get64(v.fp-0x68), v.fp-0x30)
            self.assertEqual(v.get64(v.fp-0x30), 0xa5a5a5a5a5a5a5a5)

    def test_preflight_success_and_failures(self):
        cases = [({}, 0), ({'load': -9}, -9), ({'dlsym': -7}, -7),
                 ({'null_statfs': 1}, -13641), ({'api': -12}, -12), ({'null_api': 1}, -13642)]
        for base in [0, 0x500000000]:
            for failure, expected in cases:
                v = VM(base, failure)
                self.assertEqual(v.run('mount_preflight'), expected & 0xffffffff)
                if failure.get('load', 0) < 0:
                    self.assertNotIn('original_load_api', [c[0] for c in v.calls])
                if not failure:
                    self.assertEqual(v.run('mount_preflight'), 0)
                    self.assertEqual(sum(c[0] == 'ps_load' for c in v.calls), 1)

    def test_ioctl_size_copyback_and_error(self):
        for base in [0, 0x500000000]:
            for result in [0, -1]:
                v = VM(base)
                original = bytes(range(128)) + b'NEIGHBOR-GUARD!!'
                v.u.mem_write(v.scratch, original)
                called = []
                def ioctl(args):
                    self.assertEqual(v.u.reg_read(UC_X86_REG_AL), 0)  # SysV variadic ABI
                    self.assertEqual(args[:2], [41, 0xc0845302])
                    self.assertNotEqual(args[2], v.scratch)
                    self.assertEqual(bytes(v.u.mem_read(args[2], 0x84)), bytes(range(128))+b'\0'*4)
                    v.u.mem_write(args[2], b'\x7a'*0x84)
                    called.append(args[2])
                    return result
                v.mock[base+SYMS['tu_ioctl']] = ('tu_ioctl', ioctl)
                self.assertEqual(v.run('ioctl_padded', [41, 0xc0845302, v.scratch]), result & 0xffffffff)
                self.assertEqual(bytes(v.u.mem_read(v.scratch, len(original))),
                                 (b'\x7a'*128 if result >= 0 else bytes(range(128)))+original[128:])
                self.assertEqual(bytes(v.u.mem_read(called[0], 0x90)), bytes(0x90))

    def test_indirect_wrappers_args_results_and_parent_locals(self):
        for base in [0, 0x500000000]:
            for name, offset in [('mount_first', 0x688), ('mount_second', 0x69c), ('unmount_entry', 0x6e0)]:
                for result in [0, -0x123]:
                    v = VM(base)
                    args = [111, 222, 333, 444]
                    def target(actual):
                        self.assertEqual(actual[:4], args)
                        return result
                    v.mock[base+0x50000] = ('mount_mock', target)
                    v.u.mem_write(v.fp-offset-4, b'\xa5'*12)
                    self.assertEqual(v.run(name, args, base+0x50000), result & 0xffffffff)
                    self.assertEqual(bytes(v.u.mem_read(v.fp-offset-4, 12)),
                                     b'\xa5'*4+struct.pack('<I', result & 0xffffffff)+b'\xa5'*4)
                    self.assertIn(b'mount end', b''.join(v.lines))

    def test_logger_failed_open_and_bounded_short_writes(self):
        for failure in [{'open': -2}, {'write_limit': 1}, {'write_limit': 0}]:
            v = VM(failures=failure)
            v.u.mem_write(v.scratch, b'test\0')
            v.run('log_stage', [v.scratch, -1, 0])
            self.assertLessEqual(sum(c[0] == 'ps_write' for c in v.calls), 8)
            self.assertEqual(sum(c[0] == 'ps_close' for c in v.calls), 0 if 'open' in failure else 1)

    def test_decrypt_return_and_args(self):
        v = VM(failures={'decrypt': -13})
        self.assertEqual(v.run('decrypt_logged', [111, 222]), (-13)&0xffffffff)
        self.assertEqual(next(c[1][:2] for c in v.calls if c[0] == 'original_decrypt'), [111, 222])


if __name__ == '__main__': unittest.main(verbosity=2)
