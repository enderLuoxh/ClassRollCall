param([Parameter(Mandatory=$true)][string]$Exe)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class RollCallWindows {
    public delegate bool EnumCallback(IntPtr hwnd, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
    public static IntPtr ByClass(string cls) { return FindWindow(cls,null); }
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect r);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window,StringBuilder text,int count);
    [DllImport("user32.dll",EntryPoint="GetWindowLongW")] public static extern int GetWindowLong(IntPtr window,int index);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window,int id);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr window,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll",EntryPoint="SendMessageW",CharSet=CharSet.Unicode)] public static extern IntPtr SendText(IntPtr window,uint msg,IntPtr wp,string text);
    public static bool SetWindowText(IntPtr window,string text) { return SendText(window,0x000C,IntPtr.Zero,text)!=IntPtr.Zero; }
    [DllImport("kernel32.dll")] public static extern IntPtr GlobalAlloc(uint flags,UIntPtr bytes);
    [DllImport("kernel32.dll")] public static extern IntPtr GlobalLock(IntPtr handle);
    [DllImport("kernel32.dll")] public static extern bool GlobalUnlock(IntPtr handle);
    [DllImport("kernel32.dll")] public static extern IntPtr GlobalFree(IntPtr handle);
    public static void DropFile(IntPtr window,string path) {
        byte[] text=Encoding.Unicode.GetBytes(path+"\0\0");
        byte[] data=new byte[20+text.Length];
        BitConverter.GetBytes(20).CopyTo(data,0); BitConverter.GetBytes(1).CopyTo(data,16); text.CopyTo(data,20);
        IntPtr memory=GlobalAlloc(0x42,(UIntPtr)data.Length); if(memory==IntPtr.Zero) throw new Exception("Cannot allocate file drop");
        IntPtr locked=GlobalLock(memory); Marshal.Copy(data,0,locked,data.Length); GlobalUnlock(memory);
        if(!PostMessage(window,0x0233,memory,IntPtr.Zero)) { GlobalFree(memory); throw new Exception("File drop failed"); }
    }
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumCallback callback,IntPtr data);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent,EnumCallback callback,IntPtr data);
    public static string WindowDiagnostics(uint pid) {
        var output = new StringBuilder();
        EnumWindows((h,p) => { uint owner; GetWindowThreadProcessId(h,out owner); if(owner==pid) {
            var s=new StringBuilder(2048); GetWindowText(h,s,2048); output.AppendLine("Window: "+s);
            EnumChildWindows(h,(c,q)=>{ var t=new StringBuilder(2048); GetWindowText(c,t,2048); output.AppendLine("  "+t); return true; },IntPtr.Zero);
        } return true; },IntPtr.Zero);
        return output.ToString();
    }
}
'@
function Wait-Dialog([string]$Title) {
    for ($attempt=0; $attempt -lt 100; $attempt++) {
        $dialog = [RollCallWindows]::FindWindow('ClassRollCall.Native.Dialog',$Title)
        if ($dialog -ne [IntPtr]::Zero -and [RollCallWindows]::IsWindowVisible($dialog)) { return $dialog }
        Start-Sleep -Milliseconds 100
    }
    [RollCallWindows]::WindowDiagnostics($instance.Id) | Write-Output
    throw "Dialog did not appear: $Title"
}
function Send-Command([IntPtr]$Window,[int]$Id,[int]$Notification=0) {
    [void][RollCallWindows]::PostMessage($Window,0x0111,[IntPtr](($Notification -shl 16) -bor $Id),[IntPtr]0)
}
function Review-Image([IntPtr]$Window,[string]$Label) {
    if ($env:CRC_UI_REVIEW -ne '1') { return }
    Add-Type -AssemblyName System.Drawing
    Start-Sleep -Milliseconds 400
    $rect = New-Object RollCallWindows+Rect
    [void][RollCallWindows]::GetWindowRect($Window,[ref]$rect)
    $bitmap = New-Object Drawing.Bitmap ($rect.Right-$rect.Left),($rect.Bottom-$rect.Top)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $stream = New-Object IO.MemoryStream
    try {
        $graphics.CopyFromScreen($rect.Left,$rect.Top,0,0,$bitmap.Size)
        $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
        Write-Output ('UI_IMAGE_' + $Label + ':' + [Convert]::ToBase64String($stream.ToArray()))
    } finally { $graphics.Dispose(); $bitmap.Dispose(); $stream.Dispose() }
}
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
        $floating = [RollCallWindows]::ByClass('ClassRollCall.Native.Float')
        if ($floating -ne [IntPtr]::Zero) {
            if ([RollCallWindows]::IsWindowVisible($floating)) { break }
            # Start-Process supplies SW_HIDE for the first ShowWindow call. Use the
            # application's normal restore message, then wait for UI initialization.
            [void][RollCallWindows]::PostMessage($floating,0x8002,[IntPtr]0,[IntPtr]0)
        }
        if ($instance.HasExited) { throw "Application exited: $($instance.ExitCode)" }
        Start-Sleep -Milliseconds 100
    }
    if ($floating -eq [IntPtr]::Zero) {
        [RollCallWindows]::WindowDiagnostics($instance.Id) | Write-Output
        throw 'Floating window missing'
    }
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
        $result = [RollCallWindows]::ByClass('ClassRollCall.Native.Result')
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
    $floatRect = New-Object RollCallWindows+Rect
    [void][RollCallWindows]::GetWindowRect($floating,[ref]$floatRect)
    if (($floatRect.Right-$floatRect.Left) -ne [int](108*$dpi/96) -or ($floatRect.Bottom-$floatRect.Top) -ne [int](68*$dpi/96)) { throw 'Floating button dimensions do not scale' }
    Send-Command $floating 10
    $manager = Wait-Dialog '管理班级名单'
    $saveRect = New-Object RollCallWindows+Rect
    [void][RollCallWindows]::GetWindowRect([RollCallWindows]::GetDlgItem($manager,1),[ref]$saveRect)
    if (($saveRect.Bottom-$saveRect.Top) -lt [int](48*$dpi/96)) { throw 'Touch action button is too small' }
    Review-Image $manager 'MANAGER'
    Send-Command $manager 101
    $editor = Wait-Dialog '编辑班级名单'
    [void][RollCallWindows]::SetWindowText([RollCallWindows]::GetDlgItem($editor,100),'界面测试班')
    [void][RollCallWindows]::SetWindowText([RollCallWindows]::GetDlgItem($editor,101),"张三`r`n李四`r`n王五")
    Review-Image $editor 'EDITOR'
    Send-Command $editor 1
    Start-Sleep -Milliseconds 400
    if ([RollCallWindows]::SendMessage([RollCallWindows]::GetDlgItem($manager,100),0x018B,[IntPtr]0,[IntPtr]0).ToInt32() -ne 2) { throw 'Class editor did not save' }
    Send-Command $manager 1
    Start-Sleep -Milliseconds 400
    $fixture = (Resolve-Path -LiteralPath 'tests/fixtures/generated/多列 多工作表.xlsx').Path
    [RollCallWindows]::DropFile($floating,$fixture)
    $preview = Wait-Dialog '导入预览'
    [void][RollCallWindows]::SendMessage([RollCallWindows]::GetDlgItem($preview,101),0x014E,[IntPtr]1,[IntPtr]0)
    Send-Command $preview 101 1
    Start-Sleep -Milliseconds 400
    if ([RollCallWindows]::SendMessage([RollCallWindows]::GetDlgItem($preview,104),0x018B,[IntPtr]0,[IntPtr]0).ToInt32() -ne 4) { throw 'Preview sheet selection or duplicate preservation failed' }
    Review-Image $preview 'PREVIEW'
    [void][RollCallWindows]::SendMessage([RollCallWindows]::GetDlgItem($preview,106),0x00F1,[IntPtr]1,[IntPtr]0)
    Send-Command $preview 106
    Start-Sleep -Milliseconds 200
    if ([RollCallWindows]::SendMessage([RollCallWindows]::GetDlgItem($preview,104),0x018B,[IntPtr]0,[IntPtr]0).ToInt32() -ne 3) { throw 'Preview deduplication failed' }
    [void][RollCallWindows]::SetWindowText([RollCallWindows]::GetDlgItem($preview,100),'界面导入测试')
    Send-Command $preview 1
    Start-Sleep -Milliseconds 400
    $imported = Get-Content -LiteralPath (Join-Path $appPath 'classes.ini') -Encoding utf8 | Where-Object { $_ -like '*=界面导入测试' }
    if (!$imported) { throw 'Preview confirmation did not persist the class' }
    $importedId = $imported.Split('=')[0]
    $importedNames = @(Get-Content -LiteralPath (Join-Path $appPath "rosters\$importedId.txt") -Encoding utf8)
    if ($importedNames.Count -ne 3) { throw 'Wrong confirmed import roster' }
    Send-Command $floating 40
    $about = Wait-Dialog 'ClassRollCall'
    Send-Command $about 1
    Start-Sleep -Milliseconds 200
    $second = Start-Process -FilePath $exePath -WindowStyle Hidden -PassThru -Wait
    if ($second.ExitCode -ne 0) { throw 'Second instance did not exit cleanly' }
    [void][RollCallWindows]::PostMessage($floating,0x0010,[IntPtr]0,[IntPtr]0)
    if (!$instance.WaitForExit(5000)) { throw 'Application did not exit' }
    $settingsPath = Join-Path $appPath 'settings.ini'
    [IO.File]::WriteAllText($settingsPath,([IO.File]::ReadAllText($settingsPath).Replace('initialized=1','initialized=0')))
    $instance = Start-Process -FilePath $exePath -WindowStyle Hidden -PassThru
    $welcome = Wait-Dialog '首次运行设置'
    Review-Image $welcome 'WELCOME'
    if ([RollCallWindows]::SendMessage([RollCallWindows]::GetDlgItem($welcome,102),0x00F0,[IntPtr]0,[IntPtr]0).ToInt32() -ne 0) { throw 'Startup must remain opt-in' }
    Send-Command $welcome 1
    Start-Sleep -Milliseconds 400
    $floating = [RollCallWindows]::ByClass('ClassRollCall.Native.Float')
    [void][RollCallWindows]::PostMessage($floating,0x0010,[IntPtr]0,[IntPtr]0)
    if (!$instance.WaitForExit(5000)) { throw 'Application did not exit after setup' }
    $report = [ordered]@{ cpu_ms_in_2s=$cpuMs; idle_working_set_bytes=$workingSet; threads=$threads; exe_bytes=(Get-Item -LiteralPath $exePath).Length; dpi=$dpi; gui_balanced_round=$seen; single_instance='passed'; auto_dismiss='passed'; no_activate='passed'; result_click_through='passed'; class_editor='passed'; import_preview='passed'; duplicate_toggle='passed'; import_confirmation='passed'; setup_startup_opt_in='passed'; touch_button_height_dip=48; floating_face_dip='96x56' }
    $report | ConvertTo-Json | Tee-Object -FilePath 'smoke-results.json'
} finally {
    $env:LOCALAPPDATA=$oldLocalAppData
    if ($instance -and !$instance.HasExited) { Stop-Process -Id $instance.Id -Force }
    # Deliberately retain the GUID test directory for inspection instead of recursive deletion.
}
