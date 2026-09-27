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


def exercise(path, preference):
    raw, mem, _ = pe_image(path)
    assert hashlib.sha256(raw).hexdigest() == MILES_SHA256
    emu = Uc(UC_ARCH_X86, UC_MODE_32)
    emu.mem_map(0x21000000, (len(mem) + 4095) & ~4095)
    emu.mem_write(0x21000000, bytes(mem))
    emu.mem_map(0x30000000, 4096)
    emu.mem_map(0x31000000, 4096)
    emu.mem_map(0x60000000, 0x10000)
    put = lambda address, value: emu.mem_write(address, struct.pack('<I', value))
    put(0x2104a304 + 18 * 4, preference)
    put(0x2104a0f0, 0x1234)  # Main-thread handle used by the timer.
    put(0x2103a004, 0x31000000)  # SuspendThread
    put(0x2103a100, 0x31000010)  # ResumeThread
    put(0x2104a3cc, 0x30000000)  # One registered audio timer.
    put(0x2104a2f8, 1)
    put(0x2104a3d4, 99)
    for address in (0x2104a3d0, 0x2104a3d8, 0x2104a3c0, 0x2104a3e4):
        put(address, 0)
    put(0x30000000, 2)  # Active timer.
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
        if address not in (0x21001f10, 0x31000000, 0x31000010, 0x31000020):
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
    emu.emu_start(0x210013b0, 0, count=10000)
    assert emu.reg_read(UC_X86_REG_EIP) == 0x31000030
    return counts


if __name__ == '__main__':
    original = Path(sys.argv[1])
    blocked = exercise(original, 1)
    serviced = exercise(original, 0)
    assert blocked == {'suspend': 1, 'resume': 0, 'callback': 0}, blocked
    assert serviced == {'suspend': 0, 'resume': 0, 'callback': 1}, serviced
    print('PASS: refused running-thread suspension skips the original Miles audio callback')
    print('PASS: preference 18 = 0 services that callback without SuspendThread/ResumeThread')
