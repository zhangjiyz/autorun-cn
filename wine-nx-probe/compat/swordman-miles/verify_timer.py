#!/usr/bin/env python3
"""Exercise the untouched Miles timer code against a refused SuspendThread.

Requires Unicorn. Supply the user's exact original mss32.dll; no game data is
included. This checks the timer callback path, not Switch gameplay or playback.
"""
import hashlib
from pathlib import Path
import struct
import sys

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from build import MILES_SHA256, pe_image
from build_standalone import PATCHED_SHA256


def exercise(path, preference, patched=False, base=0x21000000, active=2):
    raw, mem, opt = pe_image(path)
    assert hashlib.sha256(raw).hexdigest() == (PATCHED_SHA256 if patched else MILES_SHA256)
    if base != 0x21000000:
        # Follow the DLL's actual HIGHLOW relocation table, like the PE loader.
        rva, size = struct.unpack_from('<II', raw, opt + 136)
        end = rva + size
        while rva < end:
            page, length = struct.unpack_from('<II', mem, rva)
            assert length >= 8 and rva + length <= end
            for offset in range(rva + 8, rva + length, 2):
                entry = struct.unpack_from('<H', mem, offset)[0]
                if entry >> 12 == 0:
                    continue
                assert entry >> 12 == 3
                target = page + (entry & 0xfff)
                value = struct.unpack_from('<I', mem, target)[0]
                struct.pack_into('<I', mem, target, (value + base - 0x21000000) & 0xffffffff)
            rva += length
    emu = Uc(UC_ARCH_X86, UC_MODE_32)
    emu.mem_map(base, (len(mem) + 4095) & ~4095)
    emu.mem_write(base, bytes(mem))
    emu.mem_map(0x30000000, 4096)
    emu.mem_map(0x31000000, 4096)
    emu.mem_map(0x60000000, 0x10000)
    put = lambda address, value: emu.mem_write(address, struct.pack('<I', value))
    put(base + 0x4a304 + 18 * 4, preference)
    put(base + 0x4a0f0, 0x1234)  # Main-thread handle used by the timer.
    put(base + 0x3a004, 0x31000000)  # SuspendThread
    put(base + 0x3a100, 0x31000010)  # ResumeThread
    put(base + 0x4a3cc, 0x30000000)  # One registered audio timer.
    put(base + 0x4a2f8, 1)
    put(base + 0x4a3d4, 99)
    for offset in (0x4a3d0, 0x4a3d8, 0x4a3c0, 0x4a3e4):
        put(base + offset, 0)
    put(0x30000000, active)
    put(0x30000004, 0x31000020)  # Audio callback.
    put(0x30000008, 0x9876)  # Callback user data.
    put(0x3000000c, 0)  # Accumulated elapsed time.
    put(0x30000010, 1000)  # Timer period in microseconds.
    sp = 0x6000ff00
    put(sp, 0x31000030)
    emu.reg_write(UC_X86_REG_ESP, sp)
    counts = {'suspend': 0, 'resume': 0, 'callback': 0}

    def hook(cpu, address, size, user):
        if address == 0x31000030:
            cpu.emu_stop()
            return
        if address not in (base + 0x1f10, 0x31000000, 0x31000010, 0x31000020):
            return
        stack = cpu.reg_read(UC_X86_REG_ESP)
        result, argc = 100, 0  # Miles clock reports one millisecond elapsed.
        if address in (0x31000000, 0x31000010):
            name = 'suspend' if address == 0x31000000 else 'resume'
            counts[name] += 1
            assert struct.unpack('<I', cpu.mem_read(stack + 4, 4))[0] == 0x1234
            result, argc = (0xffffffff if name == 'suspend' else 0), 1
        elif address == 0x31000020:
            counts['callback'] += 1
            assert struct.unpack('<I', cpu.mem_read(stack + 4, 4))[0] == 0x9876
            result, argc = 0, 1
        ret = struct.unpack('<I', cpu.mem_read(stack, 4))[0]
        cpu.reg_write(UC_X86_REG_EAX, result)
        cpu.reg_write(UC_X86_REG_ESP, stack + 4 * (argc + 1))
        cpu.reg_write(UC_X86_REG_EIP, ret)

    emu.hook_add(UC_HOOK_CODE, hook)
    emu.emu_start(base + 0x13b0, 0, count=10000)
    assert emu.reg_read(UC_X86_REG_EIP) == 0x31000030
    assert emu.reg_read(UC_X86_REG_ESP) == sp + 24  # stdcall callback with five arguments.
    return counts


if __name__ == '__main__':
    original = Path(sys.argv[1])
    blocked = exercise(original, 1)
    serviced = exercise(original, 0)
    assert blocked == {'suspend': 1, 'resume': 0, 'callback': 0}, blocked
    assert serviced == {'suspend': 0, 'resume': 0, 'callback': 1}, serviced
    print('PASS: refused running-thread suspension skips the original Miles audio callback')
    print('PASS: preference 18 = 0 services that callback without SuspendThread/ResumeThread')
    if len(sys.argv) > 2:
        fixed = Path(sys.argv[2])
        for base in (0x21000000, 0x22000000):
            for preference in (0, 1, 0xffffffff):
                assert exercise(fixed, preference, patched=True, base=base) == serviced
                assert exercise(fixed, preference, patched=True, base=base, active=1) == {
                    'suspend': 0, 'resume': 0, 'callback': 0}
        print('PASS: complete DLL services active timers at original/relocated bases regardless of preference 18')
        print('PASS: stopped timers stay stopped; callback arguments and stdcall stack preserved')
