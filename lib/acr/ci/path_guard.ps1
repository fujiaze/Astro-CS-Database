# ACR path guard 脚本 (PowerShell 7)
# 每次提交前检查 git diff 是否越界（只允许 lib/acr/、工程控制/tasks/acr/、工程控制/evidence/acr/）
# 用法: pwsh -File lib/acr/ci/path_guard.ps1 [-Repo <path>]
# 退出码: 0=通过, 1=越界
#requires -Version 7

param(
    [string]$Repo = "."
)

Set-Location $Repo
git config core.quotepath false | Out-Null

# 用 git pathspec exclusion 直接得到越界文件（git 自身处理 UTF-8 中文路径）
# 输出 = 所有改动文件 减去 允许的前缀
# 注：tools/_* 为 AGENTS.md §5.4 定义的临时工具配置文件（tools/_<task_id>_<purpose>.json/.txt），使用后删除，不计入越界
$Violations = git diff --name-only HEAD -- . ':(exclude)lib/acr/' ':(exclude)工程控制/tasks/acr/' ':(exclude)工程控制/evidence/acr/' ':(exclude)tools/_*'
$Untracked = git ls-files --others --exclude-standard -- . ':(exclude)lib/acr/' ':(exclude)工程控制/tasks/acr/' ':(exclude)工程控制/evidence/acr/' ':(exclude)tools/_*'

$allViolations = @()
if ($Violations) { $allViolations += $Violations }
if ($Untracked) { $allViolations += $Untracked }

# 去重
$allViolations = $allViolations | Sort-Object -Unique

if ($allViolations.Count -eq 0) {
    Write-Host "[path_guard] OK: All changes within allowed ACR paths." -ForegroundColor Green
    exit 0
} else {
    Write-Host "[path_guard] VIOLATION: Following files are outside allowed ACR paths:" -ForegroundColor Red
    foreach ($v in $allViolations) {
        Write-Host "  $v" -ForegroundColor Red
    }
    Write-Host "[path_guard] Allowed: lib/acr/, 工程控制/tasks/acr/, 工程控制/evidence/acr/" -ForegroundColor Yellow
    Write-Host "[path_guard] ABORT. If already committed, run 'git reset HEAD~1'." -ForegroundColor Yellow
    exit 1
}
