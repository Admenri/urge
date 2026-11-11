param(
  [string]$Game = "$PSScriptRoot/../../build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe",
  [string]$Ruby = 'ruby',
  [switch]$Demo,
  [switch]$PrepareOnly
)
$ErrorActionPreference = 'Stop'
$taskRepo = (Resolve-Path "$PSScriptRoot/../..").Path
$taskDirectory = Join-Path $taskRepo $(if ($Demo) { 'test/ui/demo' } else { 'build/ui-regression' })
New-Item -ItemType Directory -Force -Path "$taskDirectory/Data" | Out-Null
Copy-Item -LiteralPath $Game -Destination "$taskDirectory/Game.exe" -Force
@'
[Game]
Title=URGE UI Demo
Scripts=Data/Scripts.rxdata
RGSS=1
[GFX]
Backend=opengl
SpriteBatch=true
PresentSampler=linear
[Display]
Width=900
Height=640
VSync=false
Fullscreen=false
FrameSkip=false
BackgroundRunning=true
'@ | Set-Content -LiteralPath "$taskDirectory/Game.ini" -Encoding utf8
$taskScript = if ($Demo) { "$PSScriptRoot/demo.rb" } else { "$PSScriptRoot/regression.rb" }
if ($Demo) {
  & $Ruby "$PSScriptRoot/pack.rb" $taskScript "$taskDirectory/Data/Scripts.rxdata" "$PSScriptRoot/demo_theme.rb"
} else {
  & $Ruby "$PSScriptRoot/pack.rb" $taskScript "$taskDirectory/Data/Scripts.rxdata"
}
if ($LASTEXITCODE -ne 0) { throw 'Ruby script packing failed' }
if ($PrepareOnly) {
  Write-Output "Prepared $taskDirectory/Game.exe"
  return
}
if ($Demo) {
  Start-Process -FilePath "$taskDirectory/Game.exe" -WorkingDirectory $taskDirectory -WindowStyle Normal -PassThru
} else {
  $taskResult = Join-Path $taskDirectory 'regression.txt'
  if (Test-Path -LiteralPath $taskResult) { Remove-Item -LiteralPath $taskResult }
  $taskProcess = Start-Process -FilePath "$taskDirectory/Game.exe" -WorkingDirectory $taskDirectory -WindowStyle Hidden -PassThru
  if (!$taskProcess.WaitForExit(60000)) {
    Stop-Process -Id $taskProcess.Id
    throw "UI regression timed out; inspect $taskDirectory/Game.log"
  }
  if (!(Test-Path -LiteralPath $taskResult)) { throw "UI regression produced no result; inspect $taskDirectory/Game.log" }
  Get-Content -LiteralPath $taskResult
  if (Select-String -LiteralPath $taskResult -Pattern '^FAIL' -Quiet) { throw 'UI regression failed' }
}
