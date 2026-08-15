# Round3 — Science 复核

优先级 Noise/Drizzle/UPM/rejection/integration：

- Noise：空背景稳健方差与星亮度/测光散射/PSF 质量解耦（SNR-003/008/010），
  ivar 优先于 1/unc²（SNR-015），蒙特卡洛矩阵 32/32；
- Drizzle：candidate 保守（false negative=0，oracle 9003），方差传播
  公式与 scaling law（SNR-002/011/012），相邻协方差如实文档化；
- UPM：frame 绑定不变量（PR-UPM-001..010），未知帧显式失败
  （F-V19R2-UPM-002），dense=sparse 1e-12；
- Rejection：RJ-001..008 语义冻结，INVALID_* hard fail，n≤2 如实
  UNDERDETERMINED；
- Integration：状态互斥显式（OK/INVALID/NO_CANDIDATES/ALL_REJECTED/
  ZERO_VALID_WEIGHT），support reducer=max。

结论：PASS（无科学契约违背）。
