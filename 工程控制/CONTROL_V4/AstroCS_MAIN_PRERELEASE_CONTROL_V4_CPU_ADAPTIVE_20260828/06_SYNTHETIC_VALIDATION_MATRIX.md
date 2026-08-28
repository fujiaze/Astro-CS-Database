# 文档驱动的合成验证矩阵

## 固定目录结构

每组算法必须建立：

```text
tools/validation/<group>/
  contract.json
  generate.py
  oracle.py
  run.py
  README.md
tests/generated/<group>/
```

- `contract.json`：SCI/ALG/API IDs、seed、输入域、单位、容差、invariants、规模档。
- `generate.py`：只生成输入和真值参数，不调用生产算法。
- `oracle.py`：独立解析/高精度/朴素参考，不链接生产库。
- `run.py`：构建/调用候选、比较所有输出层、写 JSON；所有外部进程有 timeout。
- 容差必须在首次结果前 commit；修改容差必须独立 commit并说明科学理由。

## SYN-001：Calibration / Photometry / WCS / PSF

- bias/dark/flat 解析帧，含负值、NaN/Inf、坏 flat、曝光缩放；
- 线性测光比例、零点偏移、已知星通量；
- TAN/SIP 已知星表正反投影和边界；
- Gaussian/Moffat PSF、混合星、饱和/截断/背景梯度；
- exact mask/ID，浮点按各 SCI 容差。

## SYN-002：Noise / SNR

- Gaussian、Poisson+read-noise、heteroscedastic、blank sky、离群点；
- 解析 variance/ivar/SNR；Monte Carlo 只用于统计覆盖并固定 seed；
- 验证单位、variance floor、NaN/Inf、0/负 variance 和 fallback。

## SYN-003：Drizzle

- 常量天空面亮度、常量源像素积分通量必须区分；
- 单点源/双点源 flux conservation；
- 平坦场、线性球面梯度、极区/跨经度边界、pixfrac极值；
- signal/support/variance-or-ivar 和 coverage；
- 独立球面面积/高精度细分 Oracle。

## SYN-004：UPM

- 公共常量场、已知加性场、参考 gauge、多个连通分量；
- Huber 单/多离群、不同 uncertainty/support/quality；
- 稀疏/退化/欠定 control；
- 1 worker、autotuned workers 重复运行：结构/hash exact，浮点过容差；
- race/sanitizer 负载。

## SYN-005：Rejection / Integration

- 每种方法在样本数边界前后；
- 已知中心/尺度/离群、重复值、极端动态范围、NaN/Inf；
- accepted/rejected reason exact；
- signal/support/variance-or-ivar/rejection/valid-depth 全层；
- scalar reference 与所有 ISA variant 对比。

## SYN-006：当前 main 全链路

- 至少3帧、多 tile、不同背景/噪声/星点/离群；
- 当前候选 Stage1→HiPS→Stage2→mosaic→verify；
- 禁止加载历史 binary或历史输出；
- Linux small规模，Windows medium/large规模；
- 所有重计算通过资源监控。

## PASS

- Oracle 独立；
- 所有正式输出层存在；
- invariants 全通过；
- variant/worker 科学等价；
- 无事后容差修改；
- 资源利用门禁通过。

