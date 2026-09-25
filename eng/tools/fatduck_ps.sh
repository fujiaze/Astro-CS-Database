#!/bin/bash
# 在 Fatduck（Windows 节点）上执行 PowerShell 的辅助脚本。
# 为什么要 base64：三层 ssh + 引号 + PowerShell 变量符号叠加，直接内联必然被 shell 吃掉。
# 用法：eng/tools/fatduck_ps.sh '<powershell 命令>'
set -euo pipefail
PS_CMD="$1"
B64=$(printf '%s' "$PS_CMD" | iconv -f UTF-8 -t UTF-16LE | base64 -w0)
timeout 180 ssh -o StrictHostKeyChecking=no -o ConnectTimeout=15 root@100.73.70.16 \
  "ssh -o StrictHostKeyChecking=no -o ConnectTimeout=15 fujia@100.104.10.71 \"pwsh -NoProfile -EncodedCommand $B64\"" 2>&1