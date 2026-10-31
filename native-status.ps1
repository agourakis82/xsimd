param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [switch]$Smoke,
    [switch]$RequireSqrt
)
$ErrorActionPreference = 'Stop'
$resolved = (Resolve-Path -LiteralPath $Exe).Path
$out = (Resolve-Path -LiteralPath $OutputDir).Path
$cases = @(
    @{Name='help'; Args='--help'},
    @{Name='sqrt'; Args='--no-colors=true --test-case="*complex sqrt signed zero*"'},
    @{Name='complement'; Args='--no-colors=true --test-case-exclude="*complex sqrt signed zero*"'},
    @{Name='full'; Args='--no-colors=true'}
)
if ($Smoke) { $cases = @(@{Name='smoke'; Args=''}) }
$results = @()
$failed = $false
foreach ($case in $cases) {
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo.FileName = $resolved
    $p.StartInfo.Arguments = $case.Args
    $p.StartInfo.UseShellExecute = $false
    $p.StartInfo.RedirectStandardOutput = $true
    $p.StartInfo.RedirectStandardError = $true
    $p.StartInfo.WorkingDirectory = (Get-Location).Path
    $record = @{Name=$case.Name; Arguments=$case.Args; Executable=$resolved}
    try {
        [void]$p.Start()
        $stdout = $p.StandardOutput.ReadToEndAsync()
        $stderr = $p.StandardError.ReadToEndAsync()
        if (-not $p.WaitForExit(90000)) {
            $p.Kill()
            $p.WaitForExit()
            $record.Timeout = $true
            $failed = $true
        }
        $stdout.Result | Set-Content -LiteralPath (Join-Path $out ($case.Name + '.stdout'))
        $stderr.Result | Set-Content -LiteralPath (Join-Path $out ($case.Name + '.stderr'))
        $record.SignedExit = $p.ExitCode
        $record.HexExit = '0x' + $p.ExitCode.ToString('X8')
        if ($p.ExitCode -ne 0) { $failed = $true }
        if ($RequireSqrt -and $case.Name -eq 'sqrt') {
            $record.NonemptySelection = [regex]::IsMatch($stdout.Result, 'test cases:\s+[1-9]')
            if (-not $record.NonemptySelection) { $failed = $true }
        }
    } catch {
        $record.LaunchError = $_.Exception.ToString()
        $failed = $true
    } finally {
        $p.Dispose()
    }
    $results += $record
    $record | ConvertTo-Json -Compress | Write-Output
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $out 'native-status.json')
if ($failed) { exit 1 }
