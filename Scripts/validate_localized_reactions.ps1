param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8\Engine',
    [switch]$SkipPlayer
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'WEVORA.uproject'
$logRoot = Join-Path $projectRoot 'Saved/Logs'
New-Item -ItemType Directory -Force $logRoot | Out-Null
$runStamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$reportPath = Join-Path $projectRoot "Saved/Automation/Localized_$runStamp"
$protectedFiles = @(git -C $projectRoot ls-files Content Config)
$originalHashes = @{}
foreach ($relative in $protectedFiles) {
    $originalHashes[$relative] = (Get-FileHash -LiteralPath (Join-Path $projectRoot $relative) -Algorithm SHA256).Hash
}
function Invoke-ValidationTask([string]$Executable, [string[]]$TaskArguments, [string]$Name) {
    $logPath = Join-Path $logRoot ($Name + '.log')
    & $Executable @TaskArguments *> $logPath
    if ($LASTEXITCODE -ne 0) { throw "$Name failed ($LASTEXITCODE). See $logPath" }
    Write-Host "$Name succeeded: $logPath"
}
$build = Join-Path $EngineRoot 'Build/BatchFiles/Build.bat'
$editor = Join-Path $EngineRoot 'Binaries/Win64/UnrealEditor-Cmd.exe'
$flags = @('Win64','Development',"-Project=$projectFile",'-WaitMutex','-NoHotReloadFromIDE',
    '-NoPCH','-NoSharedPCH','-NoUBA','-UBANoDetour','-nocache','-MaxParallelActions=4')
Invoke-ValidationTask $build (@('WEVORAEditor') + $flags) 'LocalizedFinalEditorBuild'
Invoke-ValidationTask $editor @($projectFile,'-unattended','-nop4','-nosplash','-nosound','-NullRHI',
    '-ExecCmds=Automation RunTests WEVORA.','-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$reportPath", "-abslog=$logRoot/LocalizedFinalAutomation.log") 'LocalizedFinalTests'
$report = Get-Content (Join-Path $reportPath 'index.json') -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.succeeded -lt 45) {
    throw "Incomplete/failed automation report: $reportPath"
}
foreach ($test in $report.tests) {
    if ($test.state -ne 'Success' -or $test.errors -gt 0 -or $test.warnings -gt 0) {
        throw "Test has errors/warnings: $($test.fullTestPath)"
    }
}
$baseline = Get-Content (Join-Path $projectRoot 'Docs/CombatPlaygroundAutomationResults.json') -Raw | ConvertFrom-Json
foreach ($oldTest in $baseline.tests) {
    if (-not ($report.tests | Where-Object { $_.fullTestPath -eq $oldTest.fullTestPath -and $_.state -eq 'Success' })) {
        throw "Baseline test missing or failed: $($oldTest.fullTestPath)"
    }
}
Write-Host "All $($baseline.tests.Count) baseline tests retained; report: $reportPath"
Invoke-ValidationTask $editor @($projectFile,'-run=CompileAllBlueprints','-unattended','-nop4','-nosound','-NullRHI') 'LocalizedFinalBlueprints'
if (-not $SkipPlayer) {
    Invoke-ValidationTask $build (@('WEVORA') + $flags) 'LocalizedFinalPlayerBuild'
    $archivePath = Join-Path $projectRoot 'Saved/LocalizedReactionsPlayer'
    Invoke-ValidationTask (Join-Path $EngineRoot 'Build/BatchFiles/RunUAT.bat') @('BuildCookRun',
        "-project=$projectFile",'-nop4','-unattended','-utf8output','-platform=Win64','-clientconfig=Development',
        '-skipbuild','-skipbuildeditor','-nocompileeditor','-cook',
        '-map=/Game/WEVORA/Maps/L_CombatPlayground+/Game/ThirdPerson/Lvl_ThirdPerson',
        '-stage','-pak','-package','-archive',"-archivedirectory=$archivePath") 'LocalizedFinalPackage'
    $playerLog = Join-Path $logRoot 'LocalizedFinalPlayerRun.log'
    $playerFile = Join-Path $archivePath 'Windows/WEVORA/Binaries/Win64/WEVORA.exe'
    if ((Get-FileHash -LiteralPath $playerFile).Hash -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot 'Binaries/Win64/WEVORA.exe')).Hash) {
        throw 'Archive does not contain the final Player binary.'
    }
    $player = Start-Process $playerFile -ArgumentList @(
        '-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-benchmark','-seconds=15',
        '-ExecCmds="set WEVORACombatPlayground bDebugOverlay true, Shot showui"', "-abslog=$playerLog"
    ) -PassThru -WindowStyle Hidden
    if (-not $player.WaitForExit(180000)) { throw 'Player did not exit in three minutes; inspect it before retrying.' }
    if ($player.ExitCode -ne 0) { throw "Player failed: $($player.ExitCode)" }
    if (-not (Select-String -Path $playerLog -Pattern 'Bringing World /Game/WEVORA/Maps/L_CombatPlayground' -Quiet)) {
        throw 'Player did not start the Playground world.'
    }
    Write-Host "Player rendering smoke completed: $playerLog (no human input/game-feel verification)."
}
foreach ($relative in $protectedFiles) {
    if ($originalHashes[$relative] -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot $relative) -Algorithm SHA256).Hash) {
        throw "Protected Content/Config changed: $relative"
    }
}
Write-Host "Protected Content/Config: $($protectedFiles.Count) SHA-256 hashes unchanged."
