param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8\Engine',
    [switch]$SkipPlayer
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'WEVORA.uproject'
$logRoot = Join-Path $projectRoot 'Saved/Logs'
New-Item -ItemType Directory -Force $logRoot | Out-Null
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
Invoke-ValidationTask $build (@('WEVORAEditor') + $flags) 'PlaygroundReproEditorBuild'
$reportPath = Join-Path $projectRoot 'Saved/Automation/PlaygroundRepro'
Invoke-ValidationTask $editor @($projectFile,'-unattended','-nop4','-nosplash','-nosound','-NullRHI',
    '-ExecCmds=Automation RunTests WEVORA.','-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$reportPath") 'PlaygroundReproTests'
$report = Get-Content (Join-Path $reportPath 'index.json') -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.succeeded -lt 33) {
    throw 'Incomplete/failed automation report; inspect test entries.'
}
foreach ($test in $report.tests) {
    if ($test.state -ne 'Success' -or $test.errors -gt 0 -or $test.warnings -gt 0) {
        throw "Test has errors/warnings: $($test.fullTestPath)"
    }
}
Invoke-ValidationTask $editor @($projectFile,'-run=CompileAllBlueprints','-unattended','-nop4','-nosound','-NullRHI') 'PlaygroundReproBlueprints'
if ($SkipPlayer) { return }
Invoke-ValidationTask $build (@('WEVORA') + $flags) 'PlaygroundReproPlayerBuild'
$archivePath = Join-Path $projectRoot 'Saved/CombatPlaygroundPlayer'
Invoke-ValidationTask (Join-Path $EngineRoot 'Build/BatchFiles/RunUAT.bat') @('BuildCookRun',
    "-project=$projectFile",'-nop4','-unattended','-utf8output','-platform=Win64','-clientconfig=Development',
    '-skipbuild','-skipbuildeditor','-nocompileeditor','-cook',
    '-map=/Game/WEVORA/Maps/L_CombatPlayground+/Game/ThirdPerson/Lvl_ThirdPerson',
    '-stage','-pak','-package','-archive',"-archivedirectory=$archivePath") 'PlaygroundReproPackage'
$playerLog = Join-Path $logRoot 'PlaygroundReproPlayerRun.log'
$player = Start-Process (Join-Path $archivePath 'Windows/WEVORA/Binaries/Win64/WEVORA.exe') -ArgumentList @(
    '-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-benchmark','-seconds=15',"-abslog=$playerLog"
) -PassThru -WindowStyle Hidden
if (-not $player.WaitForExit(180000)) { throw 'Player did not exit in three minutes; inspect it before retrying.' }
if ($player.ExitCode -ne 0) { throw "Player failed: $($player.ExitCode)" }
if (-not (Select-String -Path $playerLog -Pattern 'Bringing World /Game/WEVORA/Maps/L_CombatPlayground' -Quiet)) {
    throw 'Player did not start the Playground world.'
}
Write-Host "Player rendering smoke completed. Inspect $playerLog. This does not verify input, appearance or game feel."
