# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Independent Windows DXBC checks. Does not execute shaders or launch a game."""
import ctypes as c
import os
from pathlib import Path
import re


class Compiler:
    def __init__(self):
        if os.name != 'nt': raise RuntimeError('Native DXBC checks require Windows.')
        self.path=Path(os.environ['SystemRoot'])/'System32/d3dcompiler_47.dll'
        self.dll=c.WinDLL(str(self.path))
        self.dll.D3DCompile.argtypes=[c.c_void_p,c.c_size_t,c.c_char_p,c.c_void_p,c.c_void_p,c.c_char_p,c.c_char_p,c.c_uint,c.c_uint,c.POINTER(c.c_void_p),c.POINTER(c.c_void_p)]
        self.dll.D3DCompile.restype=c.c_long
        self.dll.D3DDisassemble.argtypes=[c.c_void_p,c.c_size_t,c.c_uint,c.c_char_p,c.POINTER(c.c_void_p)]
        self.dll.D3DDisassemble.restype=c.c_long

    @staticmethod
    def take(pointer):
        if not pointer: return b''
        table=c.cast(pointer,c.POINTER(c.POINTER(c.c_void_p))).contents
        release=c.WINFUNCTYPE(c.c_ulong,c.c_void_p)(table[2])
        try:
            size=c.WINFUNCTYPE(c.c_size_t,c.c_void_p)(table[4])(pointer)
            if size > 32*1024*1024: raise RuntimeError('Native shader output exceeds its bound.')
            address=c.WINFUNCTYPE(c.c_void_p,c.c_void_p)(table[3])(pointer)
            return c.string_at(address,size)
        finally: release(pointer)

    def compile(self,source,entry,target):
        code,error=c.c_void_p(),c.c_void_p()
        result=self.dll.D3DCompile(source,len(source),b'synthetic',None,None,entry,target,0,0,c.byref(code),c.byref(error))
        try: diagnostics=self.take(error)
        finally: output=self.take(code)
        if result < 0: raise RuntimeError('Synthetic shader compilation failed: '+diagnostics.decode('utf-8',errors='replace'))
        return output

    def disassemble(self,blob):
        if not 32 <= len(blob) <= 32*1024*1024: raise RuntimeError('Invalid native shader input size.')
        output=c.c_void_p()
        result=self.dll.D3DDisassemble(blob,len(blob),0,None,c.byref(output))
        data=self.take(output)
        if result < 0 or not data: raise RuntimeError('D3DDisassemble rejected shader: '+hex(result & 0xffffffff))
        assembly=data.rstrip(b'\0').decode('utf-8')
        stages=re.findall(r'^\s*((?:vs|ps)_[45]_0)\s*$',assembly,re.MULTILINE)
        if len(stages)!=1: raise RuntimeError('Native shader stage is absent or ambiguous.')
        return assembly,stages[0]
