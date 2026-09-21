#!/usr/bin/env bash
# 端到端真实数据 mosaic：在给定 λs 下跑真实 L4 帧（子集或全部），成品层测台阶。
# 用法: bash run_mosaic_ls.sh <lambda> <tag> [n_frames]
set -eu
cd "/workspace/Astro CS Database"
export TMPDIR=/dev/shm/astrocs_lambda
LS=$1; TAG=$2; NF=${3:-49}
OUT=run/RELEASE-02/smooth-lambda/mosaic_$TAG
CFG=run/RELEASE-02/smooth-lambda/cfg_$TAG.json
mkdir -p "$OUT"
python3 - "$LS" "$NF" "$OUT" "$CFG" <<PYEOF
import json, sys
ls, nf, out, cfg = float(sys.argv[1]), int(sys.argv[2]), sys.argv[3], sys.argv[4]
src = json.load(open("run/RELEASE-02/L4-rebuild/mosaic_49.json"))
paths = src["hips_paths"][:nf]
d = {"schema_version": "1", "hips_paths": paths, "output_dir": "/workspace/Astro CS Database/"+out,
     "weight_mode": 1, "algorithm_rejection_method": "",
     "upm": {"smoothing_lambda": ls, "max_iterations": 60}}
json.dump(d, open(cfg, "w"), indent=1)
print("cfg", cfg, "n_frames", len(paths), "lambda", ls)
PYEOF
echo "[run] lambda=$LS tag=$TAG"
/usr/bin/time -f "WALL=%e RSS=%M" build/astrocs mosaic --json "$CFG" -y > run/RELEASE-02/smooth-lambda/logs_$TAG.events.jsonl 2> run/RELEASE-02/smooth-lambda/logs_$TAG.stderr || echo "rc=$?"
tail -2 run/RELEASE-02/smooth-lambda/logs_$TAG.stderr
python3 实验/SCI-C/code/reverse_verify/smooth_lambda/seam_product.py "$OUT" "$TAG" || true
echo "MOSAIC_DONE $TAG"
