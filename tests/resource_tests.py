"""Check that Windows itself can find and extract packaged EXE resources."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import struct
import sys

k=C.WinDLL('kernel32',use_last_error=True)
s=C.WinDLL('shell32',use_last_error=True)
u=C.WinDLL('user32',use_last_error=True)
v=C.WinDLL('version',use_last_error=True)
k.LoadLibraryExW.argtypes=[W.LPCWSTR,W.HANDLE,W.DWORD]
k.LoadLibraryExW.restype=W.HMODULE
k.FindResourceW.argtypes=[W.HMODULE,C.c_void_p,C.c_void_p]
k.FindResourceW.restype=W.HANDLE
k.LoadResource.argtypes=[W.HMODULE,W.HANDLE]
k.LoadResource.restype=W.HANDLE
k.SizeofResource.argtypes=[W.HMODULE,W.HANDLE]
k.LockResource.argtypes=[W.HANDLE]
k.LockResource.restype=C.c_void_p
k.FreeLibrary.argtypes=[W.HMODULE]
s.ExtractIconExW.argtypes=[W.LPCWSTR,C.c_int,C.POINTER(W.HANDLE),C.POINTER(W.HANDLE),W.UINT]
u.DestroyIcon.argtypes=[W.HANDLE]
v.GetFileVersionInfoSizeW.argtypes=[W.LPCWSTR,C.POINTER(W.DWORD)]

def main():
    path=str(Path(sys.argv[1]).resolve())
    module=k.LoadLibraryExW(path,None,0x2|0x20)
    assert module,C.WinError(C.get_last_error())
    def resource(kind,name=1):
        handle=k.FindResourceW(module,C.c_void_p(name),C.c_void_p(kind))
        assert handle,f'Windows cannot find resource type {kind}, id {name}'
        size=k.SizeofResource(module,handle)
        return C.string_at(k.LockResource(k.LoadResource(module,handle)),size)
    try:
        group=resource(14)
        reserved,kind,count=struct.unpack_from('<HHH',group)
        assert reserved==0 and kind==1 and count>=6
        sizes=[]
        for i in range(count):
            width,height,_,_,planes,bits,size,ident=struct.unpack_from('<BBBBHHIH',group,6+14*i)
            assert len(resource(3,ident))==size
            sizes.append((width or 256,height or 256))
        assert (16,16) in sizes and (32,32) in sizes and (256,256) in sizes
        assert b'assembly' in resource(24)
        assert 'VS_VERSION_INFO'.encode('utf-16-le') in resource(16)
        print('PASS Windows resource lookup: icon images, manifest and version',flush=True)
    finally:
        k.FreeLibrary(module)
    large,small=W.HANDLE(),W.HANDLE()
    assert s.ExtractIconExW(path,0,C.byref(large),C.byref(small),1)>=1
    assert large.value and small.value
    u.DestroyIcon(large);u.DestroyIcon(small)
    unused=W.DWORD()
    assert v.GetFileVersionInfoSizeW(path,C.byref(unused))>0
    print('PASS Windows shell extracts the application icon and version',flush=True)
    binary=Path(path).read_bytes()
    pe=struct.unpack_from('<I',binary,0x3c)[0]
    assert binary[pe:pe+4]==b'PE\0\0'
    optional=pe+24
    magic=struct.unpack_from('<H',binary,optional)[0]
    assert magic==0x10b,'Expected x86 PE32'
    cli_rva,cli_size=struct.unpack_from('<II',binary,optional+96+14*8)
    assert cli_rva==0 and cli_size==0,'Native binary must not require .NET'
    print('PASS native x86 executable, no CLR/.NET header',flush=True)
    count=struct.unpack_from('<H',binary,pe+6)[0]
    opt_size=struct.unpack_from('<H',binary,pe+20)[0]
    sections=[]
    for i in range(count):
        offset=optional+opt_size+40*i
        virtual_size,rva,raw_size,raw=struct.unpack_from('<IIII',binary,offset+8)
        sections.append((rva,max(virtual_size,raw_size),raw))
    def file_offset(rva):
        for start,size,raw in sections:
            if start<=rva<start+size:return raw+rva-start
        raise ValueError('Invalid RVA')
    import_rva,import_size=struct.unpack_from('<II',binary,optional+96+8)
    cursor=file_offset(import_rva)
    imports=[]
    while any(binary[cursor:cursor+20]):
        name_rva=struct.unpack_from('<I',binary,cursor+12)[0]
        offset=file_offset(name_rva)
        imports.append(binary[offset:binary.index(b'\0',offset)].decode('ascii'))
        cursor+=20
    assert not any(any(tag in dll.lower() for tag in ['mscoree','vcruntime','msvcp','libstdc++','libgcc','python','qt5','qt6']) for dll in imports)
    print('IMPORTS='+','.join(sorted(imports)),flush=True)

if __name__=='__main__':
    main()
