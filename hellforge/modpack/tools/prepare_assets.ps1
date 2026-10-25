#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$InstallRoot,
    [Parameter(Mandatory)][string]$Out,
    [string]$ModRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$install = (Resolve-Path -LiteralPath $InstallRoot).Path
$mod = (Resolve-Path -LiteralPath $ModRoot).Path
$destination = [IO.Path]::GetFullPath($Out)
if (Test-Path -LiteralPath $destination) { throw 'Refusing to overwrite asset preparation output' }
$installed = Get-Content -LiteralPath (Join-Path $install 'hellforge-install.json') -Raw | ConvertFrom-Json -AsHashtable
if ([string]$installed.buildId -ne '25480438') { throw 'Unsupported HellForge game build' }
$cli = Join-Path $install 'app/cli/HellForge.Cli.exe'
$zig = Join-Path $install 'tools/zig/zig.exe'
$gameData = Join-Path $install 'game/data'
foreach ($required in @($cli, $zig, (Join-Path $gameData 'bundles.nxa'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing local input: $required" }
}
$expectedPath = Join-Path $mod 'expected-asset-resources.json'
$expected = Get-Content -LiteralPath $expectedPath -Raw | ConvertFrom-Json -AsHashtable
if ($expected.schema -ne 1 -or $expected.archives -isnot [Collections.IDictionary]) { throw 'Invalid expected asset inventory' }
$animationKey = 'A486D4045106165C:4D1C334D294DFA97'
$packageKey = 'AD9C6D9ED1E5E77A:9BA626AFA44A3AA3'
$magazineKey = 'E0A48D0BE9A7453F:02CD7321CD8445F5'
$privateNames = @('7CD9495D4A4B7477', 'F96E5890EFECAB78')
$modes = [ordered]@{}
$patches = @()
foreach ($bundle in $expected.archives.Keys) {
    if ($bundle -notmatch '^[0-9a-f]{16}$') { throw 'Invalid bundle identifier' }
    $groups = @{}
    foreach ($key in $expected.archives[$bundle].Keys) {
        if ($key -notmatch '^[0-9A-F]{16}:[0-9A-F]{16}$') { throw 'Invalid resource identifier' }
        $kind, $name = $key.Split(':')
        if ($key -eq $animationKey) { $modes['animation'] = 'animation-inputs.json'; continue }
        if ($key -eq $packageKey) { $modes['dependencies'] = 'dependency-inputs.json'; continue }
        if ($key -eq $magazineKey) { $modes['magazine'] = 'magazine-inputs.json'; continue }
        if ($name -in $privateNames) { $modes['palette'] = 'punisher-material-inputs.json'; continue }
        if (-not $groups.ContainsKey($kind)) { $groups[$kind] = @() }
        $groups[$kind] += "0x$name"
    }
    if ($groups.Count) {
        $resources = @($groups.Keys | Sort-Object | ForEach-Object { @{ type="0x$_"; include=@($groups[$_] | Sort-Object) } })
        $patches += @{ archives=@($bundle); resources=$resources }
    }
}
New-Item -ItemType Directory -Path $destination | Out-Null
$combined = Join-Path $destination 'data'
New-Item -ItemType Directory -Path $combined | Out-Null
function Write-Json($Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 40) + "`n", [Text.UTF8Encoding]::new($false))
}
function Invoke-Checked($Executable, [string[]]$Arguments) {
    $log = & $Executable @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Local preparation failed ($LASTEXITCODE): $log" }
}
function Build-Stock($Name, $Spec) {
    $probe = Join-Path $destination "input-specs/$Name"
    New-Item -ItemType Directory -Path $probe -Force | Out-Null
    Write-Json (Join-Path $probe 'hellforge-mod.json') @{
        format=1; id='superearth.asset-preparation'; name='Arsenal local asset preparation'; version='0.0.0'; gameplay=$true
    }
    Write-Json (Join-Path $probe 'hellforge-data.json') $Spec
    $stock = Join-Path $destination "stock/$Name"
    Invoke-Checked $cli @('build-data', '--mod', $probe, '--game-data', $gameData, '--out', $stock)
    return $stock
}
function Copy-Archives($Folder) {
    foreach ($file in Get-ChildItem -LiteralPath $Folder -File -Filter '*.patch_*') {
        $target = Join-Path $combined $file.Name
        if (Test-Path -LiteralPath $target) { throw "Generated archive name collision: $($file.Name)" }
        [IO.File]::Copy($file.FullName, $target, $false)
    }
}
if ($patches.Count) {
    $stock = Build-Stock 'unchanged' @{ format=1; patches=$patches }
    Copy-Archives $stock
}
if ($modes.Count) {
    $toolDir = Join-Path $destination 'tools'
    New-Item -ItemType Directory -Path $toolDir | Out-Null
    $exe = Join-Path $toolDir 'native_asset_prepare.exe'
    Invoke-Checked $zig @('cc','-std=c11','-Wall','-Wextra','-Werror','-municode',
        (Join-Path $mod 'tools/native_asset_prepare.c'),'-lbcrypt','-o',$exe)
    $sources = @{ animation='18235e0c9ec0e636'; palette='49f1972458f7ccec'; dependencies='9ba626afa44a3aa3'; magazine='49f1972458f7ccec' }
    foreach ($mode in $modes.Keys) {
        $spec = Get-Content -LiteralPath (Join-Path $mod "tools/$($modes[$mode])") -Raw | ConvertFrom-Json -AsHashtable
        $stock = Build-Stock $mode $spec
        $generated = Join-Path $destination "generated/$mode"
        New-Item -ItemType Directory -Path (Split-Path -Parent $generated) -Force | Out-Null
        Invoke-Checked $exe @("--$mode", (Join-Path $stock "$($sources[$mode]).patch_0"), '--out', $generated)
        Copy-Archives $generated
    }
}
# Read every generated resource independently of the C recipes. Nothing is staged.
function Hash-Bytes([byte[]]$Bytes) { return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)) }
function Fingerprint-Part([byte[]]$Bytes, [uint64]$Offset, [uint32]$Length) {
    if ($Length -and ($Offset -gt $Bytes.Length -or $Length -gt $Bytes.Length - $Offset)) { throw 'Resource part outside backing file' }
    $payload = [byte[]]::new($Length)
    if ($Length) { [Array]::Copy($Bytes, [long]$Offset, $payload, 0L, [long]$Length) }
    return @{ bytes=[long]$Length; sha256=(Hash-Bytes $payload) }
}
$actual = @{}
foreach ($file in Get-ChildItem -LiteralPath $combined -File | Where-Object Name -Match '\.patch_\d+$') {
    $cpu = [IO.File]::ReadAllBytes($file.FullName)
    $stream = [IO.File]::ReadAllBytes($file.FullName + '.stream')
    $gpu = [IO.File]::ReadAllBytes($file.FullName + '.gpu_resources')
    if ($cpu.Length -lt 72 -or [BitConverter]::ToUInt32($cpu, 0) -ne 0xF0000011u) { throw 'Invalid generated archive' }
    $nt = [BitConverter]::ToUInt32($cpu, 4); $nf = [BitConverter]::ToUInt32($cpu, 8)
    $end = 72L + 32L*$nt + 80L*$nf
    if ($end -gt $cpu.Length) { throw 'Truncated archive index' }
    $bundle = $file.Name.Split('.')[0]
    if (-not $actual.ContainsKey($bundle)) { $actual[$bundle] = @{} }
    for ($i=0; $i -lt $nf; $i++) {
        $at = [int](72L + 32L*$nt + 80L*$i)
        $key = '{0:X16}:{1:X16}' -f [BitConverter]::ToUInt64($cpu, $at+8), [BitConverter]::ToUInt64($cpu, $at)
        if ($key.StartsWith('A14E8DFA2CD117E2:')) { throw 'Lua resource in native asset output' }
        $offset = [BitConverter]::ToUInt64($cpu, $at+16)
        $size = [BitConverter]::ToUInt32($cpu, $at+56)
        if ($offset -lt $end -or [BitConverter]::ToUInt32($cpu, $at+76) -ne $i) { throw 'Invalid resource index' }
        $parts = @{
            cpu=(Fingerprint-Part $cpu $offset $size)
            stream=(Fingerprint-Part $stream ([BitConverter]::ToUInt64($cpu,$at+24)) ([BitConverter]::ToUInt32($cpu,$at+60)))
            gpu=(Fingerprint-Part $gpu ([BitConverter]::ToUInt64($cpu,$at+32)) ([BitConverter]::ToUInt32($cpu,$at+64)))
        }
        if ($actual[$bundle].ContainsKey($key)) { throw "Duplicate resource owner: $bundle/$key" }
        $actual[$bundle][$key] = $parts
    }
}
$matched = 0
foreach ($bundle in $expected.archives.Keys) {
    foreach ($key in $expected.archives[$bundle].Keys) {
        if (-not $actual.ContainsKey($bundle) -or -not $actual[$bundle].ContainsKey($key)) { throw "Missing generated resource: $bundle/$key" }
        foreach ($part in @('cpu','stream','gpu')) {
            $wanted = $expected.archives[$bundle][$key][$part]; $found = $actual[$bundle][$key][$part]
            if ($wanted.bytes -ne $found.bytes -or $wanted.sha256 -cne $found.sha256) { throw "Generated payload mismatch: $bundle/$key/$part" }
        }
        $matched++
    }
}
foreach ($bundle in $actual.Keys) {
    foreach ($key in $actual[$bundle].Keys) {
        if (-not $expected.archives.ContainsKey($bundle) -or -not $expected.archives[$bundle].ContainsKey($key)) { throw "Unexpected resource: $bundle/$key" }
    }
}
$recipeFiles = @('tools/prepare_assets.ps1','tools/native_asset_prepare.c','tools/magazine_recipe.inc',
    'tools/punisher_asset_dependencies.generated.h','tools/animation-inputs.json','tools/dependency-inputs.json',
    'tools/magazine-inputs.json','tools/punisher-material-inputs.json')
$recipeHashes = @{}
foreach ($name in $recipeFiles) { $recipeHashes[$name] = (Get-FileHash -LiteralPath (Join-Path $mod $name) -Algorithm SHA256).Hash }
$receipt = @{
    result='PASS'; game_build='25480438'; expected_assets_sha256=(Get-FileHash -LiteralPath $expectedPath -Algorithm SHA256).Hash
    recipe_files=$recipeHashes; raw_game_data_distributed=$false; staged=$false
    asset_parity=@{ result='PASS'; expected_resources=$matched; matched_resources=$matched; missing=@(); mismatched=@(); unexpected=@(); conflicts=@() }
    generated_files=@(Get-ChildItem -LiteralPath $combined -File | Sort-Object Name | ForEach-Object {
        @{ name=$_.Name; bytes=$_.Length; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
}
Write-Json (Join-Path $destination 'asset-preparation-verification.json') $receipt
Write-Output "Local asset preparation PASS: $matched resources. Generated data: $combined"
Write-Output 'Generated game data is local installation output; exclude it from distributed ZIPs.'
