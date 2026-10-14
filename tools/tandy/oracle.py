#!/usr/bin/env python3
"""Oraculo del motor de 3 voces: ejecuta el CODIGO ORIGINAL de la compilacion 3v (imagen = archivo desde 0x200) en unicorn (x86-16)
y devuelve la lista de escrituras a puertos (out) con el tick BIOS simulado. Es la referencia contra la que se compara el port.
Uso (biblioteca): from oracle import Oracle ; o = Oracle('alleycat3v.exe') ; o.call(0x16a0) ; o.call(0x173e) ...
Segmentos: CS = 0x1000 (imagen), DS = CS + 0x674 (el codigo de arranque suma [cs:9] = 0x674 parrafos)."""
import sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INSN, UC_HOOK_INTR, UC_HOOK_MEM_UNMAPPED
from unicorn.x86_const import *
CS = 0x1000; DS_PARA = 0x674
class Oracle:
    def __init__(self, exe, bios_tick=0):
        d = open(exe, 'rb').read()[0x200:]
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_16)
        uc.mem_map(0, 0x100000 + 0x10000)
        uc.mem_write(CS * 16, d)
        self.ds = CS + DS_PARA; self.tick = bios_tick; self.log = []; self.t_script = None; self.ports_in = {}
        uc.reg_write(UC_X86_REG_CS, CS); uc.reg_write(UC_X86_REG_DS, self.ds); uc.reg_write(UC_X86_REG_ES, self.ds); uc.reg_write(UC_X86_REG_SS, self.ds)
        uc.reg_write(UC_X86_REG_SP, 0xF000)
        uc.hook_add(UC_HOOK_INSN, self._out, None, 1, 0, UC_X86_INS_OUT)
        uc.hook_add(UC_HOOK_INSN, self._in, None, 1, 0, UC_X86_INS_IN)
        uc.hook_add(UC_HOOK_INTR, self._intr)
        self.stub = 0xFFF0                                   # CS:FFF0 = hlt (direccion de retorno)
        uc.mem_write(CS * 16 + self.stub, b'\xf4')
    def ds_read(self, off, n): return bytes(self.uc.mem_read(self.ds * 16 + off, n))
    def ds_write(self, off, b): self.uc.mem_write(self.ds * 16 + off, bytes(b))
    def _out(self, uc, port, size, value, ud):
        self.log.append(('out', port, value & ((1 << 8 * size) - 1), self.tick))
    def _in(self, uc, port, size, ud): return self.ports_in.get(port, 0)
    def _intr(self, uc, intno, ud):
        ax = uc.reg_read(UC_X86_REG_AX)
        if intno == 0x1a:
            if (ax >> 8) == 0: uc.reg_write(UC_X86_REG_DX, self.tick & 0xFFFF); uc.reg_write(UC_X86_REG_CX, (self.tick >> 16) & 0xFFFF); uc.reg_write(UC_X86_REG_AX, 0)
            else: self.log.append(('int1a', ax, 0, self.tick))
        else: self.log.append(('int', intno, ax, self.tick))
    def call(self, off, max_insn=1000000):
        uc = self.uc
        sp = uc.reg_read(UC_X86_REG_SP) - 2
        uc.mem_write(self.ds * 16 + sp, (self.stub).to_bytes(2, 'little'))   # SS=DS
        uc.reg_write(UC_X86_REG_SP, sp)
        uc.emu_start(CS * 16 + off, CS * 16 + self.stub, count=max_insn)
        uc.reg_write(UC_X86_REG_SP, 0xF000)
    def take(self):
        l, self.log = self.log, []; return l
if __name__ == '__main__':
    o = Oracle(sys.argv[1]); o.ds_write(1, b'\x01'); o.ds_write(0, b'\xff')
    o.call(0x16a0); print(o.take())
    for t in range(1, 6):
        o.tick = t; o.call(0x173e); print(t, o.take())
