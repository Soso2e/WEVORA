param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8\Engine',
    [ValidateSet('A','B','C','D')][string]$Phase = 'D',
    [switch]$SkipPlayer
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'WEVORA.uproject'
$logRoot = Join-Path $projectRoot 'Saved/Logs'
New-Item -ItemType Directory -Force $logRoot | Out-Null
$reportPath = Join-Path $projectRoot "Saved/Automation/Solo$Phase"
function Invoke-Task([string]$Executable, [string[]]$TaskArguments, [string]$Name) {
    & $Executable @TaskArguments *> (Join-Path $logRoot "$Name.log")
    if ($LASTEXITCODE -ne 0) { throw "$Name failed ($LASTEXITCODE). See Saved/Logs/$Name.log" }
    Write-Host "$Name succeeded"
}
$build = Join-Path $EngineRoot 'Build/BatchFiles/Build.bat'
$editor = Join-Path $EngineRoot 'Binaries/Win64/UnrealEditor-Cmd.exe'
$flags = @('Win64','Development',"-Project=$projectFile",'-WaitMutex','-NoHotReloadFromIDE',
    '-NoPCH','-NoSharedPCH','-NoUBA','-UBANoDetour','-nocache','-MaxParallelActions=4')
Invoke-Task $build (@('WEVORAEditor') + $flags) "Solo${Phase}EditorBuild"
Invoke-Task $editor @($projectFile,'-unattended','-nop4','-nosplash','-nosound','-NullRHI',
    '-ExecCmds=Automation RunTests WEVORA.','-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$reportPath", "-abslog=$logRoot/Solo${Phase}Automation.log") "Solo${Phase}Tests"
$report = Get-Content (Join-Path $reportPath 'index.json') -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.succeeded -lt 47) { throw 'Incomplete or failed automation report' }
foreach ($test in $report.tests) {
    if ($test.state -ne 'Success' -or $test.errors -gt 0 -or $test.warnings -gt 0) { throw "Test error/warning: $($test.fullTestPath)" }
}
$baseline = Get-Content (Join-Path $projectRoot 'Docs/LocalizedElementalAutomationResults.json') -Raw | ConvertFrom-Json
foreach ($oldTest in $baseline.tests) {
    if (-not ($report.tests | Where-Object { $_.fullTestPath -eq $oldTest.fullTestPath -and $_.state -eq 'Success' })) {
        throw "Prior test missing/failed: $($oldTest.fullTestPath)"
    }
}
Write-Host "All $($report.succeeded) tests passed; all $($baseline.tests.Count) prior tests retained."
if ($Phase -ne 'D') { return }
$persistenceSlot = 'WEVORA_Automation_' + [guid]::NewGuid().ToString('N')
foreach ($mode in @('Write','Read')) {
    $processReport = Join-Path $projectRoot "Saved/Automation/SoloPersistence$mode"
    Invoke-Task $editor @($projectFile,'-unattended','-nop4','-nosplash','-nosound','-NullRHI',
        '-ExecCmds=Automation RunTests WEVORA.Solo.ProcessPersistence','-TestExit=Automation Test Queue Empty',
        "-SoloPersistenceMode=$mode", "-SoloPersistenceSlot=$persistenceSlot", "-ReportExportPath=$processReport") "SoloPersistence$mode"
    $processResult = Get-Content (Join-Path $processReport 'index.json') -Raw | ConvertFrom-Json
    if ($processResult.succeeded -ne 1 -or $processResult.failed -ne 0 -or $processResult.notRun -ne 0) { throw "Process persistence $mode failed" }
}
Invoke-Task $editor @($projectFile,'-run=CompileAllBlueprints','-unattended','-nop4','-nosound','-NullRHI') 'SoloDBlueprints'
if ($SkipPlayer) { return }
Invoke-Task $build (@('WEVORA') + $flags) 'SoloDPlayerBuild'
$archive = Join-Path $projectRoot 'Saved/SoloArenaPlayer'
Invoke-Task (Join-Path $EngineRoot 'Build/BatchFiles/RunUAT.bat') @('BuildCookRun',"-project=$projectFile",
    '-nop4','-unattended','-utf8output','-platform=Win64','-clientconfig=Development',
    '-skipbuild','-skipbuildeditor','-nocompileeditor','-cook',
    '-map=/Game/WEVORA/Maps/L_SoloArenaPrototype+/Game/WEVORA/Maps/L_CombatPlayground+/Game/ThirdPerson/Lvl_ThirdPerson',
    '-stage','-pak','-package','-archive',"-archivedirectory=$archive") 'SoloDPackage'
$player = Join-Path $archive 'Windows/WEVORA/Binaries/Win64/WEVORA.exe'
if ((Get-FileHash -LiteralPath $player).Hash -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot 'Binaries/Win64/WEVORA.exe')).Hash) {
    throw 'Archived binary is stale'
}
Write-Host "Packaged Player: $player (human input acceptance remains separate)."
foreach ($map in @('L_SoloArenaPrototype','L_CombatPlayground')) {
    $playerLog = Join-Path $logRoot "SoloDPlayer_$map.log"
    $process = Start-Process $player -ArgumentList @("/Game/WEVORA/Maps/$map",'-windowed','-ResX=1280','-ResY=720',
        '-unattended','-nosound','-benchmark','-seconds=15',"-abslog=$playerLog") -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(180000)) { throw "Player did not exit: $map" }
    if ($process.ExitCode -ne 0) { throw "Player failed: $map ($($process.ExitCode))" }
    if (-not (Select-String -LiteralPath $playerLog -Pattern "Bringing World /Game/WEVORA/Maps/$map" -Quiet)) { throw "Wrong world: $map" }
    Write-Host "Rendering smoke passed: $map (no human input verification)."
}
