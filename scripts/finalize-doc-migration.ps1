$ErrorActionPreference = 'Stop'

$repo = 'D:\CreationSuite-Workspaces\CreationSuite-Codex'
$wiki = Join-Path $repo 'wiki'

$accidentalWikiTestCopy = Join-Path $wiki 'Apps\CreationEngine\Language\tests'
if (Test-Path $accidentalWikiTestCopy) {
    Remove-Item -LiteralPath $accidentalWikiTestCopy -Recurse -Force
}

$dirsToDelete = @(
    'D:\CreationSuite-Workspaces\CreationSuite-Codex\apps\CreationMovie\docs',
    'D:\CreationSuite-Workspaces\CreationSuite-Codex\apps\CreationStation\docs',
    'D:\CreationSuite-Workspaces\CreationSuite-Codex\apps\CreationStation\architecture',
    'D:\CreationSuite-Workspaces\CreationSuite-Codex\apps\CreationStation\wiki'
)

foreach ($dir in $dirsToDelete) {
    if (Test-Path $dir) {
        Remove-Item -LiteralPath $dir -Recurse -Force
    }
}

Write-Output '--- wiki status ---'
git -C $wiki status --short
Write-Output '--- remaining app doc dirs ---'
Get-ChildItem -Path (Join-Path $repo 'apps') -Recurse -Directory |
    Where-Object { $_.Name -in @('docs', 'architecture', 'wiki') -and $_.FullName -notmatch '\\third_party\\' } |
    Select-Object -ExpandProperty FullName
