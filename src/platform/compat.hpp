#pragma once
// Win8+ pointer-input declarations kept local so the binary can still target
// Windows 7 SP1 (_WIN32_WINNT=0x0601): the application never imports these
// entry points statically, it resolves them with GetProcAddress at runtime.
#include <windows.h>

#ifndef WM_POINTERUPDATE
#define WM_POINTERUPDATE 0x0245
#endif
#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN 0x0246
#endif
#ifndef WM_POINTERUP
#define WM_POINTERUP 0x0247
#endif
#ifndef WM_POINTERCAPTURECHANGED
#define WM_POINTERCAPTURECHANGED 0x0249
#endif
#ifndef GET_POINTERID_WPARAM
#define GET_POINTERID_WPARAM(wParam) (LOWORD(wParam))
#endif

#if !defined(WINVER) || WINVER < 0x0602
typedef enum tagPOINTER_INPUT_TYPE_LOCAL {
    PT_POINTER_LOCAL = 0x00000001,
    PT_TOUCH_LOCAL = 0x00000002,
    PT_PEN_LOCAL = 0x00000003,
    PT_MOUSE_LOCAL = 0x00000004,
} POINTER_INPUT_TYPE_LOCAL;
#define PT_TOUCH PT_TOUCH_LOCAL
#endif

#if !defined(POINTER_FLAG_CANCELED)
#define POINTER_FLAG_NONE 0x00000000
#define POINTER_FLAG_CANCELED 0x00008000
#define POINTER_FLAG_INCONTACT 0x00000004
#define POINTER_FLAG_FIRSTBUTTON 0x00000010
#define POINTER_FLAG_PRIMARY 0x00002000
#endif

// Layout must match the documented POINTER_INFO structure exactly: GetPointerInfo
// fills it in place and takes no size argument.
#if !defined(WINVER) || WINVER < 0x0602
typedef struct tagPOINTER_INFO_LOCAL {
    DWORD pointerType;
    UINT32 pointerId;
    UINT32 frameId;
    DWORD pointerFlags;
    HANDLE sourceDevice;
    HWND hwndTarget;
    POINT ptPixelLocation;
    POINT ptHimetricLocation;
    POINT ptPixelLocationRaw;
    POINT ptHimetricLocationRaw;
    DWORD dwTime;
    UINT32 historyCount;
    INT32 InputData;
    DWORD dwKeyStates;
    ULONGLONG PerformanceCount;
    DWORD ButtonChangeType;
} POINTER_INFO_LOCAL;
typedef POINTER_INFO_LOCAL POINTER_INFO;
#endif
