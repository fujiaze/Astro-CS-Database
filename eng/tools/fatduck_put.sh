#!/bin/bash
# 把本地文件写到 Fatduck（Windows 节点）。
# 为什么要分块：整文件 base64 会超出 Windows 命令行长度上限（实测 9 KB 即被拒）。
# 用法：eng/tools/fatduck_put.sh <本地文件> <Windows 绝对路径>
set -euo pipefail
SRC="$1"; DST="$2"
HERE=$(cd "$(dirname "$0")" && pwd)
B64=$(base64 -w0 "$SRC")
CHUNK=1200
TOTAL=$(printf %s "$B64" | wc -c)
OFF=0; FIRST=1
while [ "$OFF" -lt "$TOTAL" ]; do
  PART=$(printf %s "$B64" | cut -c$((OFF+1))-$((OFF+CHUNK)))
  OFF=$((OFF+CHUNK))
  if [ "$FIRST" = 1 ]; then MODE=Create; FIRST=0; else MODE=Append; fi
  PS="\$b='$PART'; \$p='$DST'; \$m=[IO.FileMode]::$MODE; \$fs=[IO.File]::Open(\$p,\$m); \$by=[Convert]::FromBase64String(\$b); \$fs.Write(\$by,0,\$by.Length); \$fs.Close()"
  "$HERE/fatduck_ps.sh" "$PS" > /dev/null 2>&1 || { echo "chunk failed at offset $OFF"; exit 1; }
done
PS="(Get-Item -LiteralPath '$DST').Length"
"$HERE/fatduck_ps.sh" "$PS" 2>&1 | tail -1