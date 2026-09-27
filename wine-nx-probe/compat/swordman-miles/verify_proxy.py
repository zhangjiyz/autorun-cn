#!/usr/bin/env python3
"""Run the built i386 proxy in Unicorn with controlled Win32/Miles APIs."""
from pathlib import Path
import struct
import sys

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from build import pe_image


def exercise(path, available=True):
    raw, mem, opt = pe_image(path)
    u32 = lambda offset: struct.unpack_from('<I', mem, offset)[0]
    string = lambda offset: bytes(mem[offset:]).split(b'\0', 1)[0].decode('ascii')
    base = struct.unpack_from('<I', raw, opt + 28)[0]
    emu = Uc(UC_ARCH_X86, UC_MODE_32)
    emu.mem_map(base, (len(mem) + 4095) & ~4095)
    emu.mem_write(base, bytes(mem))
    emu.mem_map(0x31000000, 0x10000)
    emu.mem_write(0x3100fff0, b'\xc3')
    emu.mem_map(0x60000000, 0x10000)
    put = lambda address, value: emu.mem_write(address, struct.pack('<I', value))
    api = {}
    descriptor = struct.unpack_from('<I', raw, opt + 104)[0]
    while u32(descriptor):
        lookup, iat = u32(descriptor), u32(descriptor + 16)
        index = 0
        while u32(lookup + index * 4):
            name = string(u32(lookup + index * 4) + 2)
            address = 0x31000000 + len(api) * 16
            api[address] = name
            put(base + iat + index * 4, address)
            index += 1
        descriptor += 20
    assert set(api.values()) == {'GetModuleHandleA', 'LoadLibraryA', 'GetProcAddress', 'SetLastError'}
    miles = {'_AIL_startup@0': 0x31001000, '_AIL_set_preference@8': 0x31001010}
    api.update({value: name for name, value in miles.items()})
    prefs, state = {18: 1, 15: 42}, {'error': 5, 'started': 0}

    def read_string(address):
        result = bytearray()
        while True:
            char = emu.mem_read(address + len(result), 1)[0]
            if not char:
                return result.decode('ascii')
            result.append(char)

    argc = {'GetModuleHandleA': 1, 'LoadLibraryA': 1, 'GetProcAddress': 2,
            'SetLastError': 1, '_AIL_startup@0': 0, '_AIL_set_preference@8': 2}

    def hook(cpu, address, size, user):
        if address == 0x3100fff0:
            cpu.emu_stop()
            return
        if address not in api:
            return
        name = api[address]
        sp = cpu.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<' + 'I' * argc[name], cpu.mem_read(sp + 4, argc[name] * 4)) if argc[name] else ()
        result = 1
        if name in ('GetModuleHandleA', 'LoadLibraryA'):
            assert read_string(args[0]) == 'mss32_autorun_original.dll'
            result = 0x21000000 if available else 0
        elif name == 'GetProcAddress':
            assert args[0] == 0x21000000
            result = miles[read_string(args[1])]
        elif name == 'SetLastError':
            state['error'] = args[0]
        elif name == '_AIL_startup@0':
            state['started'] += 1
            result = 7
        elif name == '_AIL_set_preference@8':
            result = prefs.get(args[0], 0)
            prefs[args[0]] = args[1]
        ret = struct.unpack('<I', cpu.mem_read(sp, 4))[0]
        cpu.reg_write(UC_X86_REG_EAX, result)
        cpu.reg_write(UC_X86_REG_ESP, sp + 4 * (argc[name] + 1))
        cpu.reg_write(UC_X86_REG_EIP, ret)

    emu.hook_add(UC_HOOK_CODE, hook)
    exp = struct.unpack_from('<I', raw, opt + 96)[0]
    functions = {}
    for i in range(u32(exp + 24)):
        name = string(u32(u32(exp + 32) + i * 4))
        ordinal = struct.unpack_from('<H', mem, u32(exp + 36) + i * 2)[0]
        functions[name] = base + u32(u32(exp + 28) + ordinal * 4)

    def invoke(name, *args):
        sp = 0x6000ff00
        put(sp, 0x3100fff0)
        for i, value in enumerate(args):
            put(sp + (i + 1) * 4, value)
        emu.reg_write(UC_X86_REG_ESP, sp)
        emu.emu_start(functions[name], 0, count=10000)
        assert emu.reg_read(UC_X86_REG_EIP) == 0x3100fff0
        assert emu.reg_read(UC_X86_REG_ESP) == sp + 4 * (len(args) + 1)
        return emu.reg_read(UC_X86_REG_EAX)

    if available:
        assert invoke('_AIL_startup@0') == 7
        assert state == {'error': 5, 'started': 1}
        assert prefs[18] == 0
        assert invoke('_AIL_set_preference@8', 18, 1) == 0 and prefs[18] == 0
        assert invoke('_AIL_set_preference@8', 15, 99) == 42 and prefs[15] == 99
    else:
        assert invoke('_AIL_startup@0') == 0 and state['error'] == 127
        assert invoke('_AIL_set_preference@8', 18, 1) == 0xffffffff
        assert state['error'] == 127 and state['started'] == 0


if __name__ == '__main__':
    exercise(Path(sys.argv[1]))
    exercise(Path(sys.argv[1]), available=False)
    print('PASS: built i386 startup return, stdcall stack, preference scope and no logging/file APIs')
    print('PASS: missing original library fails without calling invalid function pointers')
