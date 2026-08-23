param(
    [ValidateSet("Debug")]
    [string]$Configuration = "Debug",
    [switch]$EnableStationAsio,
    [string[]]$Targets = @("engine", "engineer", "station", "texture", "modeler", "movie", "live", "vfs", "remote"),
    [switch]$SkipSubmoduleInit
)

$ErrorActionPreference = "Stop"

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)][string]$Description,
        [Parameter(Mandatory = $true)][string[]]$Command
    )

    Write-Host ""
    Write-Host "==> $Description"
    & $Command[0] $Command[1..($Command.Length - 1)]
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $($Command -join ' ')"
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$configurationLower = $Configuration.ToLowerInvariant()

if (-not $env:JUCE_DIR) {
    throw "JUCE_DIR is not set. Set JUCE_DIR before running this script."
}

if (-not $SkipSubmoduleInit) {
    $submoduleStatus = git -C $repoRoot submodule status --recursive
    if ($LASTEXITCODE -ne 0) {
        throw "Could not read submodule status."
    }

    $dirtySubmodules = @($submoduleStatus | Where-Object { $_ -match '^[+U]' })
    if ($dirtySubmodules.Count -gt 0) {
        throw "Submodules have local branch or working-tree state that should not be overwritten by init/update. Review submodule status first, or rerun with -SkipSubmoduleInit if this checkout is already initialized."
    }

    Invoke-Step -Description "Initializing submodules" -Command @(
        "git", "-C", $repoRoot, "submodule", "update", "--init", "--recursive"
    )
}

$targetMap = @{
    "engine" = @{
        SourceDir = (Join-Path $repoRoot "apps/CreationEngine")
        BinaryDir = (Join-Path $repoRoot "apps/CreationEngine/build")
    }
    "station" = @{
        SourceDir = (Join-Path $repoRoot "apps/CreationStation")
        BinaryDir = (Join-Path $repoRoot "apps/CreationStation/build")
    }
    "engineer" = @{
        SourceDir = (Join-Path $repoRoot "apps/CreationEngineer")
        BinaryDir = (Join-Path $repoRoot "apps/CreationEngineer/build")
    }
    "texture" = @{
        SourceDir = (Join-Path $repoRoot "apps/CreationTexture")
        BinaryDir = (Join-Path $repoRoot "apps/CreationTexture/build")
    }
    "modeler" = @{
        SourceDir = (Join-Path $repoRoot "apps/CreationModeler")
        BinaryDir = (Join-Path $repoRoot "apps/CreationModeler/build")
    }
    "movie" = @{
        SourceDir = (Join-Path $repoRoot "apps/CreationMovie")
        BinaryDir = (Join-Path $repoRoot "apps/CreationMovie/build")
    }
    "live" = @{
        SourceDir = (Join-Path $repoRoot "apps/CreationLive")
        BinaryDir = (Join-Path $repoRoot "apps/CreationLive/build")
    }
    "vfs" = @{
        SourceDir = (Join-Path $repoRoot "services/VfsService")
        BinaryDir = (Join-Path $repoRoot "services/VfsService/build")
    }
    "remote" = @{
        SourceDir = (Join-Path $repoRoot "apps/CreationRemoteReceiver")
        BinaryDir = (Join-Path $repoRoot "apps/CreationRemoteReceiver/build")
    }
}

foreach ($target in $Targets) {
    if (-not $targetMap.ContainsKey($target)) {
        throw "Unknown target '$target'. Valid values: $($targetMap.Keys -join ', ')"
    }

    $entry = $targetMap[$target]
    $configureCommand = @(
        "cmake",
        "-S", $entry.SourceDir,
        "-B", $entry.BinaryDir,
        "-G", "Visual Studio 17 2022",
        "-A", "x64",
        "-DJUCE_DIR=$env:JUCE_DIR"
    )

    if ($env:CE_WINFLEXBISON_DIR) {
        $configureCommand += "-DCE_WINFLEXBISON_DIR=$env:CE_WINFLEXBISON_DIR"
    }

    if ($env:CE_LLVM_VCPKG_DIR) {
        $configureCommand += "-DCE_LLVM_VCPKG_DIR=$env:CE_LLVM_VCPKG_DIR"
    }

    if ($target -eq "station") {
        if ($EnableStationAsio) {
            $configureCommand += "-DCREATION_STATION_ENABLE_ASIO=ON"
            if ($env:CREATION_STATION_ASIO_SDK_DIR) {
                $configureCommand += "-DCREATION_STATION_ASIO_SDK_DIR=$env:CREATION_STATION_ASIO_SDK_DIR"
            }
        } else {
            $configureCommand += "-DCREATION_STATION_ENABLE_ASIO=OFF"
        }
    }

    Invoke-Step -Description "Configuring $target" -Command $configureCommand
    Invoke-Step -Description "Building $target" -Command @(
        "cmake", "--build", $entry.BinaryDir, "--config", $Configuration
    )
}
