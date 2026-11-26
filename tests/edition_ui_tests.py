"""Verify two locally built editions against their private input JSON files.

Usage: python tests/edition_ui_tests.py class1.exe class1.json class2.exe class2.json
No student names are printed, copied into the repository or sent to the network.
"""
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

import functional_ui as ui

ui.u.GetWindowTextW.argtypes=[W.HWND,W.LPWSTR,C.c_int]

def main():
    root=Path(tempfile.mkdtemp(prefix='edition-check-',dir=Path(__file__).resolve().parent.parent/'build'))
    entries=[]
    for i in (1,3):
        exe=Path(sys.argv[i]).resolve()
        spec=json.loads(Path(sys.argv[i+1]).read_text(encoding='utf8'))
        entries.append((exe,spec,root/('ClassRollCall-'+spec['id'])))
    running=[]
    try:
        for exe,spec,data in entries:
            p=ui.launch(exe,root)
            running.append(p)
            cls='ClassRollCall.Native.Float.'+spec['id']
            ui.wait_for(lambda:ui.windows(p.pid,cls))
            assert not ui.windows(p.pid,'ClassRollCall.Native.Dialog'),'Edition must start without an import/welcome dialog'
            assert 'initialized=1' in (data/'settings.ini').read_text(encoding='utf8')
            index=(data/'classes.ini').read_text(encoding='utf8').strip()
            cid,name=index.split('=',1)
            assert name==spec['class_name']
            roster=data/'rosters'/(cid+'.txt')
            assert roster.read_text(encoding='utf8').splitlines()==spec['names'],'Built-in roster must exactly match input records'
            print('PASS '+spec['id']+': built-in roster, '+str(len(spec['names']))+' records, no setup required',flush=True)
        first,second=running
        h,_=ui.windows(first.pid,'ClassRollCall.Native.Float.'+entries[0][1]['id'])[0]
        ui.u.PostMessageW(h,0x111,21,0)
        ui.wait_for(lambda:'mode=random' in (entries[0][2]/'settings.ini').read_text(encoding='utf8'))
        assert 'mode=balanced' in (entries[1][2]/'settings.ini').read_text(encoding='utf8')
        print('PASS independent profiles and concurrent instances',flush=True)
        for p,(_,spec,data) in zip(running,entries):
            h,_=ui.windows(p.pid,'ClassRollCall.Native.Float.'+spec['id'])[0]
            ui.post(h,0x201,30,30,1)
            ui.post(h,0x202,30,30)
            ui.wait_for(lambda:ui.windows(p.pid,'ClassRollCall.Native.Result'))
            time.sleep(1.35)
            result,_=ui.windows(p.pid,'ClassRollCall.Native.Result')[0]
            text=C.create_unicode_buffer(512)
            ui.u.GetWindowTextW(result,text,len(text))
            assert text.value in spec['names'],'Result must belong to the correct class'
            ui.wait_for(lambda:not ui.windows(p.pid,'ClassRollCall.Native.Result'),5)
            ui.post(h,0x10)
            assert p.wait(timeout=4)==0
        running.clear()
        for exe,spec,data in entries:
            p=ui.launch(exe,root)
            running.append(p)
            h,_=ui.wait_for(lambda:ui.windows(p.pid,'ClassRollCall.Native.Float.'+spec['id']))[0]
            assert not ui.windows(p.pid,'ClassRollCall.Native.Dialog')
            cid=(data/'classes.ini').read_text(encoding='utf8').split('=',1)[0]
            assert (data/'rosters'/(cid+'.txt')).read_text(encoding='utf8').splitlines()==spec['names']
            ui.post(h,0x10)
            assert p.wait(timeout=4)==0
        print('PASS correct-class results, clean exit and restart for both editions',flush=True)
    finally:
        for p in running:
            if p.poll() is None:
                p.terminate();p.wait(timeout=5)
        print('Isolated data: '+str(root),flush=True)

if __name__=='__main__':
    main()
