$ErrorActionPreference = 'Stop'

$repo = 'D:\CreationSuite-Workspaces\CreationSuite-Codex'
$wiki = Join-Path $repo 'wiki'
$appsRoot = Join-Path $repo 'apps'

if (-not (Test-Path $wiki)) {
    throw "Wiki repo not found at $wiki"
}

$docDirNames = @('docs', 'architecture', 'wiki', 'config', 'deploy', 'tests')

$files = Get-ChildItem -Path $appsRoot -Recurse -File |
    Where-Object {
        $_.FullName -notmatch '\\third_party\\' -and
        $_.FullName -notmatch '\\build\\' -and
        $_.FullName -notmatch '\\vcpkg_installed\\' -and
        $_.FullName -notmatch '\\Language\\tests\\' -and
        $_.Name -notin @('README.md', 'AGENTS.md') -and
        (
            ($_.Directory.FullName -split '\\') | Where-Object { $docDirNames -contains $_ }
        )
    }

foreach ($file in $files) {
    $relative = $file.FullName.Substring($appsRoot.Length).TrimStart('\')
    $dest = Join-Path $wiki (Join-Path 'Apps' $relative)
    $destDir = Split-Path -Parent $dest
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    Copy-Item -LiteralPath $file.FullName -Destination $dest -Force
}

$cleanupDirs = @('docs', 'architecture', 'wiki', 'config', 'deploy', 'tests')
$dirs = Get-ChildItem -Path $appsRoot -Recurse -Directory |
    Where-Object { $cleanupDirs -contains $_.Name } |
    Sort-Object FullName -Descending

foreach ($dir in $dirs) {
    $remainingFiles = @(Get-ChildItem -Path $dir.FullName -Recurse -File -ErrorAction SilentlyContinue)
    if ($remainingFiles.Count -eq 0) {
        Remove-Item -LiteralPath $dir.FullName -Recurse -Force
        continue
    }

    $nonDocFiles = @($remainingFiles | Where-Object { $_.Extension -notin @('.md', '.markdown') })
    if ($nonDocFiles.Count -eq 0) {
        Remove-Item -LiteralPath $dir.FullName -Recurse -Force
    }
}

Write-Output '--- moved files ---'
$files | ForEach-Object { $_.FullName }
Write-Output '--- wiki status ---'
git -C $wiki status --short
Write-Output '--- remaining doc dirs ---'
Get-ChildItem -Path $appsRoot -Recurse -Directory |
    Where-Object { $cleanupDirs -contains $_.Name } |
    Select-Object -ExpandProperty FullName
