# Compiles every shader in shaders\ the way the game does, and fails on any error or any warning. This is the gate shaders get:
# neither of our code scanners can read them. The game compiles them from text files as it starts, so a mistake here would
# otherwise only show on a player's machine, and only as a shader that's missing.
#
# Each file is compiled on its own. A .fxh file isn't a shader: it's included by others, so it's checked as part of each one that
# includes it. Nothing is kept, because the game makes its own copy each time it starts.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# Microsoft's shader compiler comes with the Windows SDK. Newest first, by the version in its folder's name.
$fxc = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x86\fxc.exe" |
    Sort-Object { [version]$_.Directory.Parent.Name } |
    Select-Object -Last 1
if (-not $fxc) { throw 'The shader compiler, fxc.exe, was not found' }
"Using $($fxc.FullName)"

# The game picks shader model 2 or 3 by how a file's name starts. That rule is read from the game's own code, so there's only one
# copy of it, and this can't drift away from what the game really does.
$code = Get-Content -Raw (Join-Path $root 'win32\d3_func.cpp')
$rule = [regex]::Match($code, 'static bool ShaderNeedsModel3[^}]*\}').Value
$starts = @([regex]::Matches($rule, 'compare\(0, \d+, "(\w+)"\)') | ForEach-Object { $_.Groups[1].Value })
if ($starts.Count -eq 0) { throw 'Could not read ShaderNeedsModel3 in win32\d3_func.cpp' }
"Shader model 3 for names starting: $($starts -join ', ')"

$shaders = @(Get-ChildItem (Join-Path $root 'shaders') -File | Where-Object { $_.Extension -in '.vsh', '.psh' })
if ($shaders.Count -eq 0) { throw 'No shaders were found' }

$output = [System.IO.Path]::GetTempFileName()
$failed = 0

foreach ($shader in $shaders) {
    # The kind and the name of the first function are the two the game uses (R_CreateVertexShader, R_CreatePixelShader).
    $kind = if ($shader.Extension -eq '.vsh') { 'vs' } else { 'ps' }
    $needsModel3 = @($starts | Where-Object { $shader.Name.StartsWith($_) }).Count -gt 0
    $target = if ($needsModel3) { "${kind}_3_0" } else { "${kind}_2_0" }

    & $fxc.FullName /nologo /WX /T $target /E "${kind}_main" /Fo $output $shader.FullName

    if ($LASTEXITCODE -ne 0) {
        "FAILED  $($shader.Name) as $target"
        $failed++
    }
    else {
        "ok      $($shader.Name) as $target"
    }
}

Remove-Item $output -ErrorAction SilentlyContinue

if ($failed -ne 0) {
    throw "$failed of $($shaders.Count) shaders failed"
}

"All $($shaders.Count) shaders compiled with no warnings"
