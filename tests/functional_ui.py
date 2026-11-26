"""Windows UI regression: isolated fake data; never touches existing app processes.

Run: python tests/functional_ui.py dist/ClassRollCall.exe
"""
import ctypes as C
from ctypes import wintypes as W
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import time
import uuid

u = C.WinDLL('user32', use_last_error=True)
k = C.WinDLL('kernel32', use_last_error=True)
u.SetProcessDpiAwarenessContext.argtypes = [C.c_void_p]
u.SetProcessDpiAwarenessContext(C.c_void_p(-4))
u.PostMessageW.argtypes = [W.HWND, W.UINT, C.c_size_t, C.c_ssize_t]
u.SendMessageW.argtypes = u.PostMessageW.argtypes
u.SendMessageW.restype = C.c_ssize_t
u.GetDlgItem.argtypes = [W.HWND, C.c_int]
u.GetDlgItem.restype = W.HWND
u.SetWindowTextW.argtypes = [W.HWND, W.LPCWSTR]
k.CreateFileW.argtypes = [W.LPCWSTR,W.DWORD,W.DWORD,C.c_void_p,W.DWORD,W.DWORD,W.HANDLE]
k.CreateFileW.restype = W.HANDLE
k.CloseHandle.argtypes = [W.HANDLE]
k.GlobalAlloc.argtypes = [W.UINT, C.c_size_t]
k.GlobalAlloc.restype = W.HANDLE
k.GlobalLock.argtypes = [W.HANDLE]
k.GlobalLock.restype = C.c_void_p
k.GlobalUnlock.argtypes = [W.HANDLE]
k.GlobalFree.argtypes = [W.HANDLE]
callback = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)


def windows(pid, cls=None):
    result = []
    def visit(h, _):
        p = W.DWORD()
        u.GetWindowThreadProcessId(h, C.byref(p))
        name = C.create_unicode_buffer(256)
        u.GetClassNameW(h, name, 256)
        if p.value == pid and u.IsWindowVisible(h) and (cls is None or cls == name.value):
            r = W.RECT()
            u.GetWindowRect(h, C.byref(r))
            result.append((h, r))
        return True
    u.EnumWindows(callback(visit), 0)
    return result


def wait_for(test, timeout=4):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        result = test()
        if result:
            return result
        time.sleep(.04)
    raise AssertionError('Timed out: ' + str(test))


def post(h, msg, x=0, y=0, wp=0):
    assert u.PostMessageW(h, msg, wp, (int(y) & 65535) << 16 | (int(x) & 65535))


def set_text(h, value):
    # SetWindowText only updates server-side captions for another process.
    # WM_SETTEXT reaches the Edit control's actual text buffer.
    buf = C.create_unicode_buffer(value)
    assert u.SendMessageW(h, 0x000c, 0, C.cast(buf, C.c_void_p).value)


def launch(exe, root):
    env = dict(os.environ, LOCALAPPDATA=str(root), APPDATA=str(root))
    return subprocess.Popen([str(exe)], env=env, stdin=subprocess.DEVNULL,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                            creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP)


def drop_file(h, path):
    import struct
    payload = struct.pack('<IiiII', 20, 0, 0, 0, 1) + (str(path)+'\0\0').encode('utf-16-le')
    memory = k.GlobalAlloc(0x42, len(payload))
    assert memory
    address = k.GlobalLock(memory)
    assert address
    C.memmove(address, payload, len(payload))
    k.GlobalUnlock(memory)
    if not u.PostMessageW(h, 0x233, memory, 0):
        k.GlobalFree(memory)
        raise AssertionError('File drop failed')


def main():
    original_exe = Path(sys.argv[1]).resolve()
    root = Path(tempfile.mkdtemp(prefix='ClassRollCall-ui-', dir=Path(__file__).resolve().parent.parent/'build'))
    exe=root/'ClassRollCall-UI-Test.exe'
    binary=original_exe.read_bytes();mutex='Local\\ClassRollCall.Native.v2'.encode('utf-16-le')
    assert binary.count(mutex)==1
    exe.write_bytes(binary.replace(mutex,('Local\\ClassRollCall.Native.'+uuid.uuid4().hex[:2]).encode('utf-16-le')))
    data = root / 'ClassRollCall'
    (data / 'rosters').mkdir(parents=True)
    cid = str(uuid.uuid4())
    settings = data / 'settings.ini'
    settings.write_text(f'initialized=1\ncurrent={cid}\nmode=balanced\n', encoding='utf-8')
    (data / 'classes.ini').write_text(f'{cid}=Test class\n', encoding='utf-8')
    roster = data / 'rosters' / (cid + '.txt')
    roster.write_text('Alice\nBob\nCarol\n', encoding='utf-8')
    p = launch(exe, root)
    try:
        floating = lambda: windows(p.pid, 'ClassRollCall.Native.Float')
        dialogs = lambda: windows(p.pid, 'ClassRollCall.Native.Dialog')
        menus = lambda: windows(p.pid, 'ClassRollCall.Native.Menu')
        h, r = wait_for(floating)[0]
        scale = u.GetDpiForWindow(h) / 96
        assert abs((r.right-r.left)/scale-124) <= 1, (r.right-r.left, scale)
        lock = k.CreateFileW(str(roster), 0x80000000, 0, None, 3, 0x80, None)
        assert lock != C.c_void_p(-1).value
        try:
            post(h, 0x201, 20*scale, 20*scale, 1)
            time.sleep(.06)
            post(h, 0x200, -80*scale, -80*scale, 1)
            time.sleep(.1)
            post(h, 0x202, 20*scale, 20*scale)
            wait_for(lambda: 'monitor=' in settings.read_text(encoding='utf-8'))
            assert not dialogs(), 'Drag must not open an error dialog when roster is locked'
        finally:
            k.CloseHandle(lock)
        position = floating()[0][1]
        print('PASS drag while roster locked', flush=True)

        # Click-to-pick must show a result, stop after the fade, and preserve roster data.
        post(h, 0x201, 20*scale, 20*scale, 1)
        post(h, 0x202, 20*scale, 20*scale)
        wait_for(lambda: windows(p.pid, 'ClassRollCall.Native.Result'))
        time.sleep(1.35)
        result = windows(p.pid, 'ClassRollCall.Native.Result')[0][0]
        name = C.create_unicode_buffer(256)
        u.GetWindowTextW(result, name, 256)
        assert name.value in ['Alice', 'Bob', 'Carol'], name.value
        wait_for(lambda: not windows(p.pid, 'ClassRollCall.Native.Result'), 4)
        assert roster.read_text(encoding='utf-8') == 'Alice\nBob\nCarol\n'
        print('PASS pick and result lifecycle', flush=True)

        def submenu():
            _, r = floating()[0]
            post(h, 0x7b, r.left+10, r.top+10)
            parent = wait_for(menus)[0]
            post(parent[0], 0x200, 50*scale, 25*scale)
            child = wait_for(lambda: [w for w in menus() if w[0] != parent[0]])[0]
            return parent, child

        for move_first in (False, True):
            parent, child = submenu()
            x = parent[1].left+50*scale-child[1].left
            y = parent[1].top+63*scale-child[1].top
            if move_first:
                post(child[0], 0x200, x, y)
                time.sleep(.12)
                assert menus(), 'Returning from submenu must keep parent open'
                post(parent[0], 0x201, 50*scale, 63*scale, 1)
            else:
                post(child[0], 0x201, x, y, 1)
            manager = wait_for(dialogs)[0][0]
            post(manager, 0x10)
            wait_for(lambda: not dialogs())
        print('PASS submenu direct click and hover return', flush=True)

        # Editing through the actual manager controls must persist the class and names.
        u.PostMessageW(h, 0x111, 10, 0)
        manager = wait_for(dialogs)[0][0]
        u.PostMessageW(manager, 0x111, 102, 0)
        editor = wait_for(lambda: [w for w in dialogs() if w[0] != manager])[0][0]
        set_text(u.GetDlgItem(editor, 100), 'Edited class')
        set_text(u.GetDlgItem(editor, 101), 'Dana\r\nEli\r\nEli')
        u.PostMessageW(editor, 0x111, 1, 0)
        wait_for(lambda: len(dialogs()) == 1)
        u.PostMessageW(manager, 0x111, 1, 0)
        wait_for(lambda: not dialogs())
        try:
            wait_for(lambda: 'Edited class' in (data/'classes.ini').read_text(encoding='utf-8'))
        except AssertionError:
            print('EDIT INDEX:', (data/'classes.ini').read_text(encoding='utf-8'), flush=True)
            print('EDIT DIALOGS:', dialogs(), flush=True)
            raise
        index = (data/'classes.ini').read_text(encoding='utf-8').strip()
        new_id = index.split('=')[0]
        assert (data/'rosters'/(new_id+'.txt')).read_text(encoding='utf-8') == 'Dana\nEli\nEli\n'
        print('PASS edit class and preserve duplicate names', flush=True)

        # Pick the multi-sheet workbook, not malformed or formula-only fixtures.
        fixture = next(p for p in (Path(__file__).resolve().parent/'fixtures').glob('*.xlsx') if p.stem not in ('broken','shared-formula'))
        drop_file(h, fixture)
        preview = wait_for(dialogs)[0][0]
        count = lambda: u.SendMessageW(u.GetDlgItem(preview, 104), 0x18b, 0, 0)
        assert count() == 4, 'Default preview must choose the names sheet'
        u.SendMessageW(u.GetDlgItem(preview, 106), 0xf1, 1, 0)
        u.PostMessageW(preview, 0x111, 106, 0)
        wait_for(lambda: count() == 3)
        set_text(u.GetDlgItem(preview, 100), 'Imported class')
        u.PostMessageW(preview, 0x111, 1, 0)
        wait_for(lambda: not dialogs())
        wait_for(lambda: 'Imported class' in (data/'classes.ini').read_text(encoding='utf-8'))
        row = next(s for s in (data/'classes.ini').read_text(encoding='utf-8').splitlines() if s.endswith('=Imported class'))
        assert len((data/'rosters'/(row.split('=')[0]+'.txt')).read_text(encoding='utf-8').splitlines()) == 3
        print('PASS drop XLSX, select names sheet, deduplicate and import', flush=True)

        second = launch(exe, root)
        try:
            assert second.wait(timeout=4) == 0
        finally:
            if second.poll() is None:
                second.terminate()
                second.wait(timeout=5)
        print('PASS single instance', flush=True)

        # A locked settings file must not prevent teardown, including an open menu.
        submenu()
        lock = k.CreateFileW(str(settings), 0x80000000, 0, None, 3, 0x80, None)
        assert lock != C.c_void_p(-1).value
        try:
            post(h, 0x10)
            assert p.wait(timeout=4) == 0
        finally:
            k.CloseHandle(lock)
        print('PASS exit during menu with settings locked', flush=True)
        portable=root/'data'/'ClassRollCall'
        assert (portable/'settings.ini').exists(),'Locked settings must migrate to a writable complete profile'
        assert 'Edited class' in (portable/'classes.ini').read_text(encoding='utf-8')
        assert 'Imported class' in (portable/'classes.ini').read_text(encoding='utf-8')
        settings=portable/'settings.ini'
        p = launch(exe, root)
        h, r = wait_for(floating)[0]
        assert abs(r.left-position.left) <= 1 and abs(r.top-position.top) <= 1
        post(h, 0x10)
        assert p.wait(timeout=4) == 0
        print('PASS restart restores position', flush=True)
        settings.write_text(settings.read_text(encoding='utf-8').replace('initialized=1','initialized=0'), encoding='utf-8')
        p = launch(exe, root)
        welcome = wait_for(dialogs)[0][0]
        assert u.SendMessageW(u.GetDlgItem(welcome, 102), 0xf0, 0, 0) == 0
        u.PostMessageW(welcome, 0x111, 1, 0)
        wait_for(lambda: not dialogs())
        h, _ = wait_for(floating)[0]
        post(h, 0x10)
        assert p.wait(timeout=4) == 0
        assert 'initialized=1' in settings.read_text(encoding='utf-8')
        print('PASS first run keeps startup opt-in', flush=True)
    finally:
        if p.poll() is None:
            p.terminate()
            p.wait(timeout=5)
        print('Isolated test data: ' + str(root), flush=True)


if __name__ == '__main__':
    main()
