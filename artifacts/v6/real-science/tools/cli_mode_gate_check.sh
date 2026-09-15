#!/usr/bin/env bash
# REAL-SCIENCE-001 — CLI 模式门负向检查（RUNTIME-CI-001 v6 mode gate）
# 期望：production/baseline rc=0；psf_snr_power/auto/support_x_snr2/legacy "0|1|2"/unknown rc=2。
set -u
cd "$(dirname "$0")/../../../.." || exit 99
ROOT=$(pwd)
BIN="$ROOT/build/astrocs"
TMP="$ROOT/run/v6/real-science/tmp"
mkdir -p "$TMP"
CFG="$TMP/base_config.json"
"$BIN" config init --output "$CFG" >/dev/null 2>&1
OUT="$ROOT/run/v6/real-science/logs/06_cli_mode_gate.jsonl"
: > "$OUT"
FAIL=0
emit() { printf '{"check":"%s","mode":"%s","rc":%d,"expected_rc":%d,"pass":%s}\n' "$1" "$2" "$3" "$4" "$([ "$3" -eq "$4" ] && echo true || echo false)" >> "$OUT"; [ "$3" -eq "$4" ] || FAIL=1; }
for m in point_information surface_gls psfsw_robust equal pixel_ivar; do
  timeout 60 "$BIN" phase2 validate --config "$CFG" --mode "$m" >"$TMP/gate_$m.out" 2>&1; emit "phase2_cli_mode" "$m" $? 0
done
for m in psf_snr_power auto support_x_snr2 0 1 2 bogus; do
  timeout 60 "$BIN" phase2 validate --config "$CFG" --mode "$m" >"$TMP/gate_$m.out" 2>&1; emit "phase2_cli_mode_reject" "$m" $? 2
done
for w in 0 1 2; do
  python3 -c "import json;json.dump({'schema_version':'1','inputs':{'lights':[],'darks':[],'flats':[],'bias':[]},'output_dir':'.','weight_mode':$w},open('$TMP/cfg_w$w.json','w'))"
  if [ "$w" = "0" ]; then exp=2; else exp=0; fi
  timeout 60 "$BIN" phase2 validate --config "$TMP/cfg_w$w.json" >"$TMP/gate_cfg_w$w.out" 2>&1; emit "phase2_config_weight_mode" "$w" $? $exp
done
# phase3 export-mode 由 C++ 负向 harness 直接对 route_phase3_mode 单源判定
# （CLI validate 需完整 phase3 config，超出本任务五口径面，避免与配置校验混淆）。
echo "CLI_MODE_GATE fail=$FAIL"
exit $FAIL
