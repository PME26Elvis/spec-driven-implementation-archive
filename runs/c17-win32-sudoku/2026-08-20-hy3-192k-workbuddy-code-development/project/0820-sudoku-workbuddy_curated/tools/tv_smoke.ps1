$ErrorActionPreference = 'Continue'
$TV = "D:\0820-sudoku-workbuddy\build\bin\tinyvcs.exe"
$W = "D:\tvsmoke"
if (Test-Path $W) { Remove-Item -Recurse -Force $W }
New-Item -ItemType Directory -Force -Path $W | Out-Null
Set-Location $W

function Run($label, [string[]]$a) {
    $out = & $TV @a 2>&1
    $code = $LASTEXITCODE
    Write-Output "--- $label => EXIT=$code"
    foreach ($l in $out) { Write-Output "    $l" }
}

Run "help" @("--help")
Run "no-args" @()
Run "bad-cmd" @("frobnicate")
Run "status-no-repo" @("status")
Run "init" @("init")
Set-Content -Path "$W\a.txt" -Value "hello" -NoNewline -Encoding ascii
Set-Content -Path "$W\b.txt" -Value "world" -NoNewline -Encoding ascii
Run "status-untracked" @("status")
Run "add-a" @("add", "a.txt")
Run "status-staged" @("status")
Run "commit1" @("commit", "-m", "first commit", "--author", "tester")
Run "log" @("log")
Run "verify" @("verify")
Run "add-all" @("add", "--all")
Run "commit2" @("commit", "-m", "second commit", "--author", "tester")
Run "branch-list" @("branch")
Run "branch-new" @("branch", "feature/x")
Run "switch" @("switch", "feature/x")
Run "status-on-branch" @("status")
Run "verify2" @("verify")
