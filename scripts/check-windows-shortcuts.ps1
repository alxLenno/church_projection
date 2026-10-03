$ErrorActionPreference = 'Stop'
$installer = (Resolve-Path 'build/ChurchProjection-1.2.2-win64.exe').Path
$installDir = Join-Path $env:RUNNER_TEMP 'ChurchProjectionShortcutTest'
$process = Start-Process -FilePath $installer -ArgumentList "/S /D=$installDir" -PassThru
if (-not $process.WaitForExit(120000)) {
    Stop-Process -Id $process.Id -Force
    throw 'Silent installation timed out'
}
if ($process.ExitCode -ne 0) { throw "Installer exited with $($process.ExitCode)" }
$expectedExe = [IO.Path]::GetFullPath((Join-Path $installDir 'bin/ChurchProjection.exe'))
if (-not (Test-Path $expectedExe)) { throw 'Installed app is missing' }
$shell = New-Object -ComObject WScript.Shell
function Check-Shortcut($paths, $label) {
    foreach ($path in $paths) {
        if (Test-Path $path) {
            $shortcut = $shell.CreateShortcut($path)
            if ($shortcut.TargetPath -ieq $expectedExe -and $shortcut.IconLocation -like "$expectedExe*") {
                Write-Host "PASS: $label shortcut targets the installed app and its icon"
                return
            }
        }
    }
    throw "$label shortcut is missing or targets the wrong executable"
}
$programs = @('Programs', 'CommonPrograms') | ForEach-Object {
    Join-Path ([Environment]::GetFolderPath($_)) 'ChurchProjection 1.2.2/ChurchProjection.lnk'
}
$desktops = @('DesktopDirectory', 'CommonDesktopDirectory') | ForEach-Object {
    Join-Path ([Environment]::GetFolderPath($_)) 'ChurchProjection 1.2.2.lnk'
}
Check-Shortcut $programs 'Start menu'
Check-Shortcut $desktops 'Desktop'
