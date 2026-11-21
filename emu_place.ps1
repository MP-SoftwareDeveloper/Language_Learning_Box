# Places the Android emulator window in the TOP-LEFT QUARTER of the primary screen.
# Called by emu_test.bat. Waits (max 3 min) for the window, then moves/resizes it.
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
}
"@
[void][W]::SetProcessDPIAware()
$wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
$h = [int]($wa.Height / 2)
$w = [int][Math]::Min($wa.Width / 2, $h * 0.55)   # phone is portrait; stays inside the quarter
for ($i = 0; $i -lt 90; $i++) {
  $p = Get-Process | Where-Object { $_.MainWindowTitle -like 'Android Emulator*' } | Select-Object -First 1
  if ($p) {
    [void][W]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, $wa.X, $wa.Y, $w, $h, 0x0004)  # SWP_NOZORDER
    Start-Sleep -Seconds 3
    [void][W]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, $wa.X, $wa.Y, $w, $h, 0x0004)
    break
  }
  Start-Sleep -Seconds 2
}
