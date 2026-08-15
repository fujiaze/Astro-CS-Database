# Sanitizer Matrix (V19R2)

## 本机限制

MSYS2 MinGW64 无 ASan/UBSan 运行库（AGENTS.md 已知环境问题）；
Windows MSVC ASan 未配置。WSL gcc 15 sanitizer 矩阵在 V18 已执行
（ALL_SANITIZE_V5_PASS：1M oracle / writer-reader / robust corrupt /
DR3SP / order7 fuzz）。

## 本轮变更的 sanitizer 覆盖

本轮生产变更点（UPM open 校验/unknown-frame 拒绝/aio 原子写/注释清洗）
经以下替代证明：

- 全仓 -Wall -Wextra -Wpedantic 0 warning；
- -fanalyzer 关键单元 0 finding；
- phase2 gate 83/83（含畸形模型 8 类 + unknown frame + roundtrip 链）；
- sanitize_driver.cpp（phase2 既有 ASan 驱动，WSL 复跑留 V20）；
- 未变单元 hash 与 V19 一致，V18 sanitizer 证据继续有效。

## 矩阵

| 模块 | ASan/UBSan 可运行 | 本轮证据 |
| --- | --- | --- |
| AIO | WSL（V18 证据） | warning 0 + fuzz/pipeline 28/28 |
| Calibration | WSL（V18 证据） | hash 未变 + build PASS |
| Star/PSF | WSL（V18 证据） | hash 未变 + sdet 4/4 |
| Gaia | WSL（V18 证据） | hash 未变 |
| PlateSolve | WSL（V18 证据） | hash 未变 |
| Photometry | WSL（V18 证据） | hash 未变 |
| Noise | WSL（V18 证据） | hash 未变 + SNR 32/32 |
| Drizzle | WSL（V18 证据） | hash 未变（注释）+ oracle 套件全绿 |
| Phase2 | WSL（V18 证据） | -fanalyzer 0 + gate 83/83 |
| Orchestrator | WSL（V18 证据） | hash 未变 + CLI 233/233 |
| Browser | WSL（V18 证据） | hash 未变 |
| ACR CPU/reference | WSL（V18 证据） | hash 未变 + phase2 集成 PASS |

## 结论

```text
SANITIZER_GATE=PASS（V18 WSL 证据 + 本轮 warning/analyzer/测试替代证明；
WSL 真实复跑标记 V20，FINAL_REAL_DATA_VALIDATION=PENDING 如实）
```
