# Diagnostics Review (V19R2)

## Stage IDs（DIAGNOSTICS_STANDARD）

P1.READ / P1.CALIBRATE / P1.STAR / P1.PSF / P1.PLATESOLVE /
P1.PHOTOMETRIC / P1.NOISE / P1.DRIZZLE / P1.HIPS_WRITE /
P2.COVERAGE / P2.SAMPLER / P2.UPM / P2.REJECTION / P2.INTEGRATE /
P2.HIPS_WRITE——记录于 docs/architecture/ERROR_MODEL.md。

## Error categories

CONFIG / INPUT_CORRUPT / DEPENDENCY / NUMERIC / NO_DATA / RESOURCE /
TIMEOUT / IO / SCIENCE_GATE / INTERNAL（ERROR_HANDLING_STANDARD）。

## 本轮诊断对齐

- ERR-P2-UPM-001（畸形 UPM 模型）→ troubleshooting 条目 + 稳定 rc=1；
- F-V19R2-UPM-002：unknown frame 显式失败（rc=1/NaN），不再静默错帧；
- 退出码表 AstroCsExitCode 文档化（SUCCESS=0..INTERNAL=9）；
- tools/astrocs_diagnose.py 保留（V19 交付）。

## 结论

```text
DIAGNOSTICS_TRACEABILITY=PASS
```
