"""UI regression with fake rosters. --real --path exercises real mouse movement.
Add --touch to exercise Windows touch injection (Windows 8+ desktop session).
Uses a temporary EXE copy with a unique mutex to coexist with a running app.
"""
import ctypes as C
from ctypes import wintypes as W
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

BASE = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('ui', BASE/'tests/functional_ui.py')
ui = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ui)
u = ui.u
u.GetDpiForWindow.argtypes = [W.HWND]
u.GetDpiForWindow.restype = W.UINT
u.SetCursorPos.argtypes = [C.c_int,C.c_int]
u.mouse_event.argtypes = [W.DWORD,W.DWORD,W.DWORD,W.DWORD,C.c_size_t]

def mouse(x,y,button=None):
    if not u.SetCursorPos(int(x),int(y)):
        raise C.WinError(C.get_last_error())
    time.sleep(.12)
    if button:
        u.mouse_event(0x0002 if button=='left' else 0x0008,0,0,0,0)
        time.sleep(.06)
        u.mouse_event(0x0004 if button=='left' else 0x0010,0,0,0,0)
        time.sleep(.12)

def mouse_path(x,y):
    point=W.POINT()
    u.GetCursorPos(C.byref(point))
    for i in range(1,21):
        u.SetCursorPos(int(point.x+(x-point.x)*i/20),int(point.y+(y-point.y)*i/20))
        time.sleep(.012)

class PointerInfo(C.Structure):
    _fields_=[('pointerType',W.DWORD),('pointerId',W.UINT),('frameId',W.UINT),
              ('pointerFlags',W.DWORD),('sourceDevice',W.HANDLE),('hwndTarget',W.HWND),
              ('ptPixelLocation',W.POINT),('ptHimetricLocation',W.POINT),
              ('ptPixelLocationRaw',W.POINT),('ptHimetricLocationRaw',W.POINT),
              ('dwTime',W.DWORD),('historyCount',W.UINT),('InputData',C.c_int32),
              ('dwKeyStates',W.DWORD),('PerformanceCount',C.c_uint64),('ButtonChangeType',C.c_int)]

class TouchInfo(C.Structure):
    _fields_=[('pointerInfo',PointerInfo),('touchFlags',W.UINT),('touchMask',W.UINT),
              ('rcContact',W.RECT),('rcContactRaw',W.RECT),('orientation',W.UINT),('pressure',W.UINT)]

def touch_tests(p,h,settings,ids,scale):
    u.InitializeTouchInjection.argtypes=[W.UINT,W.DWORD]
    u.InjectTouchInput.argtypes=[W.UINT,C.POINTER(TouchInfo)]
    assert u.InitializeTouchInjection(10,3),C.WinError(C.get_last_error())
    def inject(x,y,flags):
        info=TouchInfo()
        info.pointerInfo.pointerType=2
        info.pointerInfo.pointerId=0
        info.pointerInfo.ptPixelLocation=W.POINT(int(x),int(y))
        info.pointerInfo.pointerFlags=flags
        info.touchMask=7
        info.rcContact=W.RECT(int(x)-2,int(y)-2,int(x)+2,int(y)+2)
        info.orientation=90
        info.pressure=512
        if not u.InjectTouchInput(1,C.byref(info)):
            raise OSError(f'Touch injection error {C.get_last_error()}, point={x},{y}, flags={flags:#x}, structure={C.sizeof(info)}, screen={u.GetSystemMetrics(0)},{u.GetSystemMetrics(1)}')
        time.sleep(.04)
    def tap(x,y,hold=.08):
        inject(x,y,0x10000|0x2|0x4)
        deadline=time.monotonic()+hold
        while time.monotonic()<deadline:
            inject(x,y,0x20000|0x2|0x4)
        inject(x,y,0x40000)
        time.sleep(.12)
    menus=lambda:ui.windows(p.pid,'ClassRollCall.Native.Menu')
    def open_menu():
        _,r=ui.windows(p.pid,'ClassRollCall.Native.Float')[0]
        tap(r.left+40*scale,r.top+35*scale,.65)
        return ui.wait_for(menus)[0]
    def select(group,row):
        parent=open_menu()
        tap(parent[1].left+60*scale,parent[1].top+group*scale)
        child=ui.wait_for(lambda:[w for w in menus() if w[0]!=parent[0]])[0]
        tap(child[1].left+60*scale,child[1].top+row*scale)
        ui.wait_for(lambda:not menus())
        time.sleep(.1)
    select(184,30)
    assert 'mode=balanced' in settings.read_text(encoding='utf8')
    select(184,78)
    assert 'mode=random' in settings.read_text(encoding='utf8')
    select(30,30)
    assert f'current={ids[0]}' in settings.read_text(encoding='utf8')
    select(30,78)
    assert f'current={ids[1]}' in settings.read_text(encoding='utf8')
    print('PASS injected touch long-press and mode/class taps',flush=True)
    _,r=ui.windows(p.pid,'ClassRollCall.Native.Float')[0]
    x,y=r.left+40*scale,r.top+35*scale
    inject(x,y,0x10000|0x2|0x4)
    for i in range(1,9):
        inject(x-120*scale*i/8,y-100*scale*i/8,0x20000|0x2|0x4)
    inject(x-120*scale,y-100*scale,0x40000)
    time.sleep(.2)
    _,moved=ui.windows(p.pid,'ClassRollCall.Native.Float')[0]
    assert moved.left<r.left-50*scale and moved.top<r.top-50*scale
    assert 'monitor=' in settings.read_text(encoding='utf8')
    print('PASS injected touch drag saves position',flush=True)
    tap(moved.left+40*scale,moved.top+35*scale)
    ui.wait_for(lambda:ui.windows(p.pid,'ClassRollCall.Native.Result'))
    ui.wait_for(lambda:not ui.windows(p.pid,'ClassRollCall.Native.Result'),6)
    assert not menus(),'A short tap must not leave a menu open'
    print('PASS injected touch tap-to-pick and result auto-hide',flush=True)

def main():
    root = BASE/'build'/('menu-input-'+uuid.uuid4().hex[:8])
    data = root/'ClassRollCall'
    (data/'rosters').mkdir(parents=True)
    ids = sorted(str(uuid.uuid4()) for _ in range(2))
    settings = data/'settings.ini'
    settings.write_text(f'initialized=1\ncurrent={ids[0]}\nmode=balanced\n', encoding='utf8')
    (data/'classes.ini').write_text(''.join(f'{cid}=Test {i+1}\n' for i,cid in enumerate(ids)),encoding='utf8')
    for i,cid in enumerate(ids):
        (data/'rosters'/(cid+'.txt')).write_text(f'Student {i+1}\n',encoding='utf8')
    original = Path(sys.argv[1])
    binary = original.read_bytes()
    mutex = 'Local\\ClassRollCall.Native.v2'.encode('utf-16-le')
    assert binary.count(mutex)==1,'Test requires the standard edition'
    replacement = ('Local\\ClassRollCall.Native.'+uuid.uuid4().hex[:2]).encode('utf-16-le')
    exe = root/'MenuInputTest.exe'
    exe.write_bytes(binary.replace(mutex,replacement))
    p = ui.launch(exe,root)
    try:
        floating = lambda: ui.windows(p.pid,'ClassRollCall.Native.Float')
        menus = lambda: ui.windows(p.pid,'ClassRollCall.Native.Menu')
        h,r = ui.wait_for(floating)[0]
        scale = u.GetDpiForWindow(h)/96
        def choose(group, row, side):
            ui.post(h,0x7b,u.GetSystemMetrics(0)-10 if side=='left' else 200,300)
            parent = ui.wait_for(menus)[0]
            ui.post(parent[0],0x200,50*scale,group*scale)
            child = ui.wait_for(lambda:[w for w in menus() if w[0]!=parent[0]])[0]
            if '--real' in sys.argv:
                mouse(parent[1].left+50*scale,parent[1].top+group*scale)
                child = ui.wait_for(lambda:[w for w in menus() if w[0]!=parent[0]])[0]
                if '--path' in sys.argv:
                    mouse_path(child[1].left+50*scale,child[1].top+row*scale)
                mouse(child[1].left+50*scale,child[1].top+row*scale,'left')
            else:
                ui.post(child[0],0x200,50*scale,row*scale)
                time.sleep(.08)
                ui.post(child[0],0x201,50*scale,row*scale,1)
                ui.post(child[0],0x202,50*scale,row*scale)
            ui.wait_for(lambda:not menus())
            time.sleep(.1)
        for side in ['left','right']:
            choose(184,78,side)
            assert 'mode=random' in settings.read_text(encoding='utf8'),settings.read_text(encoding='utf8')
            choose(184,30,side)
            assert 'mode=balanced' in settings.read_text(encoding='utf8')
            choose(30,78,side)
            assert f'current={ids[1]}' in settings.read_text(encoding='utf8')
            choose(30,30,side)
            assert f'current={ids[0]}' in settings.read_text(encoding='utf8')
            print('PASS mode and class selection via '+side+' submenu',flush=True)
        ui.post(h,0x201,30*scale,30*scale,1)
        ui.post(h,0x202,30*scale,30*scale)
        ui.wait_for(lambda:ui.windows(p.pid,'ClassRollCall.Native.Result'))
        ui.wait_for(lambda:not ui.windows(p.pid,'ClassRollCall.Native.Result'),6)
        print('PASS result shown and auto-hidden',flush=True)
        # Selecting a mode or class while the result is visible must work too.
        ui.post(h,0x201,30*scale,30*scale,1)
        ui.post(h,0x202,30*scale,30*scale)
        ui.wait_for(lambda:ui.windows(p.pid,'ClassRollCall.Native.Result'))
        choose(184,78,'left')
        assert 'mode=random' in settings.read_text(encoding='utf8')
        assert not ui.windows(p.pid,'ClassRollCall.Native.Result')
        ui.post(h,0x201,30*scale,30*scale,1)
        ui.post(h,0x202,30*scale,30*scale)
        ui.wait_for(lambda:ui.windows(p.pid,'ClassRollCall.Native.Result'))
        choose(30,78,'right')
        assert f'current={ids[1]}' in settings.read_text(encoding='utf8')
        print('PASS mode and class switching during an active result',flush=True)
        if '--touch' in sys.argv:
            touch_tests(p,h,settings,ids,scale)
        u.GetGuiResources.argtypes=[W.HANDLE,W.DWORD]
        def handles():
            return tuple(u.GetGuiResources(W.HANDLE(int(p._handle)),kind) for kind in (0,1))
        counts=[]
        for cycle in range(35):
            ui.post(h,0x7b,200,300)
            parent=ui.wait_for(menus)[0]
            ui.post(parent[0],0x200,50*scale,30*scale)
            child=ui.wait_for(lambda:[w for w in menus() if w[0]!=parent[0]])[0]
            ui.post(child[0],0x201,-1000,-1000,1)
            ui.wait_for(lambda:not menus())
            assert p.poll() is None
            if cycle in (4,34):counts.append(handles())
        assert all(end<=start+4 for start,end in zip(*counts)),counts
        print('PASS 35 submenu open/close cycles; GDI/USER resources stable '+str(counts),flush=True)
    finally:
        if p.poll() is None:
            for handle,_ in ui.windows(p.pid,'ClassRollCall.Native.Float'):
                ui.post(handle,0x10)
            try:
                p.wait(timeout=2)
            except subprocess.TimeoutExpired:
                p.terminate()
                p.wait(timeout=5)
        print('TEST_DATA='+str(root),flush=True)

if __name__=='__main__':
    main()


