param([Parameter(Mandatory=$true)][string]$Exe)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class RollCallWindows {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect r);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window,StringBuilder text,int count);
    [DllImport("user32.dll",EntryPoint="GetWindowLongW")] public static extern int GetWindowLong(IntPtr window,int index);
}
'@
$exePath = (Resolve-Path -LiteralPath $Exe).Path
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('ClassRollCall-smoke-' + [guid]::NewGuid().ToString('D'))
$appPath = Join-Path $testRoot 'ClassRollCall'
$oldLocalAppData = $env:LOCALAPPDATA
$instance = $null
try {
    New-Item -ItemType Directory -Path (Join-Path $appPath 'rosters') -Force | Out-Null
    $classId = [guid]::NewGuid().ToString('D')
    [IO.File]::WriteAllText((Join-Path $appPath 'settings.ini'), "initialized=1`ncurrent=$classId`nmode=balanced`nautostart=0`n")
    [IO.File]::WriteAllText((Join-Path $appPath 'classes.ini'), "$classId=冒烟测试班`n")
    [IO.File]::WriteAllText((Join-Path $appPath "rosters\$classId.txt"), "张三`n李四`n王五`n")
    $env:LOCALAPPDATA = $testRoot
    $instance = Start-Process -FilePath $exePath -WindowStyle Hidden -PassThru
    $floating = [IntPtr]::Zero
    for ($i=0; $i -lt 100; $i++) {
        $floating = [RollCallWindows]::FindWindow('ClassRollCall.Native.Float',$null)
        if ($floating -ne [IntPtr]::Zero) { break }
        if ($instance.HasExited) { throw "Application exited: $($instance.ExitCode)" }
        Start-Sleep -Milliseconds 100
    }
    if ($floating -eq [IntPtr]::Zero) { throw 'Floating window missing' }
    [uint32]$windowPid = 0
    [void][RollCallWindows]::GetWindowThreadProcessId($floating,[ref]$windowPid)
    if ($windowPid -ne $instance.Id) { throw 'Window belongs to another process' }
    if (![RollCallWindows]::IsWindowVisible($floating)) { throw 'Floating window hidden' }
    $style = [RollCallWindows]::GetWindowLong($floating,-20)
    if (($style -band 0x08000008) -ne 0x08000008) { throw 'Floating window must be topmost and no-activate' }
    Start-Sleep -Milliseconds 700
    $instance.Refresh()
    $before = $instance.TotalProcessorTime
    Start-Sleep -Seconds 2
    $instance.Refresh()
    $cpuMs = ($instance.TotalProcessorTime-$before).TotalMilliseconds
    $workingSet = $instance.WorkingSet64
    $threads = $instance.Threads.Count
    $seen = @()
    $dpi = [RollCallWindows]::GetDpiForWindow($floating)
    $point = [int](32*$dpi/96)
    $packed = [IntPtr](($point -shl 16) -bor $point)
    for ($round=0; $round -lt 3; $round++) {
        [void][RollCallWindows]::PostMessage($floating,0x0201,[IntPtr]1,$packed)
        [void][RollCallWindows]::PostMessage($floating,0x0202,[IntPtr]0,$packed)
        Start-Sleep -Milliseconds 1400
        $result = [RollCallWindows]::FindWindow('ClassRollCall.Native.Result',$null)
        if (![RollCallWindows]::IsWindowVisible($result)) { throw 'Result window missing' }
        $resultStyle = [RollCallWindows]::GetWindowLong($result,-20)
        if (($resultStyle -band 0x08000020) -ne 0x08000020) { throw 'Result must be click-through and no-activate' }
        $name = New-Object Text.StringBuilder 256
        [void][RollCallWindows]::GetWindowText($result,$name,256)
        if ($name.ToString() -notin @('张三','李四','王五')) { throw 'Final result invalid' }
        $seen += $name.ToString()
        Start-Sleep -Milliseconds 3200
        if ([RollCallWindows]::IsWindowVisible($result)) { throw 'Result did not auto-dismiss' }
    }
    if (($seen | Select-Object -Unique).Count -ne 3) { throw 'Balanced GUI round repeated a student' }
    $second = Start-Process -FilePath $exePath -WindowStyle Hidden -PassThru -Wait
    if ($second.ExitCode -ne 0) { throw 'Second instance did not exit cleanly' }
    [void][RollCallWindows]::PostMessage($floating,0x0010,[IntPtr]0,[IntPtr]0)
    if (!$instance.WaitForExit(5000)) { throw 'Application did not exit' }
    $report = [ordered]@{ cpu_ms_in_2s=$cpuMs; idle_working_set_bytes=$workingSet; threads=$threads; exe_bytes=(Get-Item -LiteralPath $exePath).Length; dpi=$dpi; gui_balanced_round=$seen; single_instance='passed'; auto_dismiss='passed'; no_activate='passed'; result_click_through='passed' }
    $report | ConvertTo-Json | Tee-Object -FilePath 'smoke-results.json'
} finally {
    $env:LOCALAPPDATA=$oldLocalAppData
    if ($instance -and !$instance.HasExited) { Stop-Process -Id $instance.Id -Force }
    # Deliberately retain the GUID test directory for inspection instead of recursive deletion.
}
