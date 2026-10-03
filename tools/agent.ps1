# Talk to a running app's Virtual Engineer (shared/VirtualEngineer/README.md) from a terminal.
#
#   tools\agent.ps1 status                      [-App texture]
#   tools\agent.ps1 ask "Why is node 4 failing?"
#   tools\agent.ps1 cards "add a struct node"    which cards a request would bring up
#   tools\agent.ps1 app graph                   one of the app's endpoints (tools\agent.ps1 app lists them)
#   tools\agent.ps1 tools                       what the engineer may do in the app, with each tool's effect
#   tools\agent.ps1 undo                        undo the last request that changed something ("undo force": also after later edits)
#
# It finds the API the way the apps find each other: the VFS root pointer, the VFS service's heartbeat in that root,
# then the app's announcement, the VFS entry agents/<app>.json. Nothing is read from or written to any other file.
param(
    [Parameter(Position = 0)][ValidateSet("status", "ask", "cards", "app", "tools", "undo")][string]$Command = "status",
    [Parameter(Position = 1)][string]$Text = "",
    [string]$App = "texture",
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = "Stop"

$pointer = Join-Path $env:APPDATA "Djehuti-Suite\suite-settings.json"
if (-not (Test-Path $pointer)) { throw "No suite VFS root is chosen yet ($pointer is missing)." }
$root = (Get-Content $pointer -Raw | ConvertFrom-Json).suiteVfsRoot
$heartbeat = Join-Path $root "VfsHeartbeat.json"
if (-not (Test-Path $heartbeat)) { throw "The VFS service is not running (no heartbeat in $root). Start any suite app." }
$vfsPort = (Get-Content $heartbeat -Raw | ConvertFrom-Json).httpPort

try {
    # The service hands entries back as bytes, not JSON.
    $raw = Invoke-WebRequest "http://127.0.0.1:$vfsPort/suite/entry?path=agents/$App.json" -UseBasicParsing
    $content = if ($raw.Content -is [byte[]]) { [Text.Encoding]::UTF8.GetString($raw.Content) } else { [string]$raw.Content }
    $announcement = $content | ConvertFrom-Json
} catch {
    throw "Djehuti $App's Virtual Engineer is not running (no agents/$App.json in the VFS)."
}
$headers = @{ Authorization = "Bearer $($announcement.token)" }
$base = $announcement.baseUrl

switch ($Command) {
    "status" {
        Invoke-RestMethod "$base/v1/status" -Headers $headers | ConvertTo-Json -Depth 6
    }
    "cards" {
        $q = [uri]::EscapeDataString($Text)
        Invoke-RestMethod "$base/v1/cards/match?q=$q" -Headers $headers | ConvertTo-Json -Depth 6
    }
    "app" {
        $path = if ($Text) { "/v1/app/$Text" } else { "/v1/app" }
        Invoke-RestMethod "$base$path" -Headers $headers | ConvertTo-Json -Depth 8
    }
    "tools" {
        (Invoke-RestMethod "$base/v1/tools" -Headers $headers).tools | ForEach-Object { "{0,-28} {1,-12} {2}" -f $_.name, $_.effect, $_.title }
    }
    "undo" {
        $body = @{ evenIfEditedSince = ($Text -eq "force") } | ConvertTo-Json
        Invoke-RestMethod "$base/v1/undo" -Method Post -Headers $headers -ContentType "application/json" -Body $body | ConvertTo-Json
    }
    "ask" {
        if (-not $Text) { throw "ask needs the message: tools\agent.ps1 ask ""...""" }
        $body = @{ content = $Text } | ConvertTo-Json
        $queued = Invoke-RestMethod "$base/v1/messages" -Method Post -Headers $headers -ContentType "application/json" -Body $body
        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        do {
            Start-Sleep -Milliseconds 500
            $reply = Invoke-RestMethod "$base/v1/requests/$($queued.requestId)" -Headers $headers
        } while ($reply.status -in @("queued", "running") -and (Get-Date) -lt $deadline)
        $reply | ConvertTo-Json -Depth 8
    }
}
