# AstroCS 已知限制（Known Limitations）

> 上游：ASTROCS_DESIGN.md §1.3（非目标）

> 口径：本页只登记**现行**限制（限制内容 + 现状 + 归属/去向），不复制历史报告原文；历史过程由 git 历史承载。
> 历史轮次节点（V19R2/V19R8 等）中**仍成立**的限制已并入下列条目，随节点失效者不再登记。
> 状态词唯一口径 = `ASTROCS_DESIGN.md` §12.5；发布结论见 `docs/RELEASE_STATUS.md` 与 `docs/owner/RELEASE_STATUS.md`。
> CLEAN-402（历史治理工件清理）并入本页的条目及其原始出处见 `run/CLEAN-402/CONCLUSION_MIGRATION.md`。

## A. 平台与验证面

1. **真实数据域**：工程/合成验收已完成；BASS + 2×2 + 3×3 与大规模真实数据的终验仍未达成（`FINAL_REAL_DATA_VALIDATION` 未过）。
2. **Windows x64 面**：`NOT_VERIFIED`——`VERIFIED` 要求正式平台（Windows x64）+ 真实数据验收通过；Linux 腿全绿不等于 Windows 绿，Windows 腿未实测。
3. **T1 数据集**：无真实数据（外部阻塞，Phase1 冻结前即记录）。
4. **ACR（CPU/GPU 异构）**：`DORMANT`——不进生产构建/加载/路由/发布。
5. **Sanitizer**：MSYS2 MinGW 无 ASan 运行库，WSL gcc 15 可跑。ASan/UBSan 运行时矩阵在 Linux 增量验证中为 `SKIP-evidenced`（以机器门 + 直接科学门禁代替），需 Windows/MSYS2 完整 toolchain 复核后方可视为全量关闭。
6. **GUI / Aladin 冒烟**：无 GUI 环境，未执行。
7. **性能快照**：现有性能基线为轻量快照（单机单次 wall 计时），未做多规模 / 火焰图完整 benchmark。

## B. 科学口径与实现限制

8. **Drizzle 相邻协方差**：pixfrac/resampling 引入，已量化（SNR-012）但未建模。
9. **HISS**：不携带 variance 产品。
10. **weight / rejection_count 诊断产品**：JSON 诊断，未输出为 Image HiPS。
11. **UPM ivar 回退**：输入帧无 ivar 产品时积分权重回退 support（`ivar_product_missing` 计数如实记录）。
12. **Phase1 SNR catalogue**：作为诊断保留，不作为科学权重。
13. **Phase1 不确定度子产品缺失（未关，P0 级）**：CLI `normalize` 产出的 `p1_final.json` 实测 `n_variance_tiles=0` / `n_ivar_tiles=0`；写出器本身具备产出 variance/ivar 子产品的能力，缺口在 p1 节点未向 PipelineFrame 注入 variance 块。**最高设计 §3.1 规定不存在「权重模式」概念（全程只有 SNR）**，故该缺口的影响面 = 绝对 SNR / 不确定度链的可达性。
14. **Phase1 HiPS signal 量纲与 BUNIT 声明不一致（待负责人裁定口径）**：signal 实为**单位立体角**量（ADU/sr，实现自述 `signal = flux/area`），而 export 平面 FITS 写 `BUNIT=ADU`。数值本身自洽（面亮度守恒），但单位声明与实际口径不符——下游若按 ADU 解读会差约 10 个量级。
15. **接缝残余未随天光面修复消失（L4 视觉复验，未关）**：全量 R 通道成品帧目检，天光面修复后接缝仍存在，幅度为本地背景的 1–3%；水平带 1 上缘由 +1.06% 变为 −2.39%（**变差 ≈2.2×**）。**根因不是 sky_plane 回退**，而是等权均值下帧集变化处的残余零点差 / 排异差异，需另行定位。接缝判据尚未通过。
16. **天光面生效后的负值像素属约定变更（非缺陷）**：加性天光面生效后背景归零，负值像素占比由 6.0e-7 升至 **0.459**（46%）；下游读取与判据需按"允许负像素"的约定解释，不得当缺陷判红。
17. **`drizzle_scale_arcsec` 合法域**：NaN / ≤0 / >824.52″ 一律 `rc=2` + `set_error`（契约侧登记；去向 `docs/contracts/DATA_SEMANTICS.md`，由前台合并）。

## C. 工程与产品面

18. **`p3_output.cpp` 原子提交为自实现**：插件文档要求"复用 infrastructure/aio 的原子提交设施"尚未兑现（该文件自实现 tmp+rename）。
19. **Phase1 产品标记载荷（D08）未关**：calibrated 产品未带 `ASTROCS*` 标记 / HISTORY。
20. **资源门回收判据在该负载下不达标**：P3-006 2600² 双线性导出确定性触发 `alloc_reclaim_missing`（`reclaim_frac=0.0` / `unexplained_residual`，peak_rss≈2.6e8），根因未修（属 IMPL/resource 面）。
21. **验收证据文件缺位**：`artifacts/evidence/prerelease-v5/ISA-005/MEASUREMENTS.csv`、`evidence/v6_1_rework/TASK_LEDGER.csv` 缺位；相应测试显式 SKIP 并标注 NOT_APPLICABLE 依据（不以 skip 充绿）。
22. **三项挂账**：plate_solve `n_inliers` 门限（实测 38 < 40）、dll_loader Win32 工具例外、PSF 无独立单测。
23. **CAT-GAIA 两项**：KI-1 `query_cache.out_mag` 非 bitwise 复现；KI-2 混合 DB 光谱未 memcpy。
24. **声明路径与实现的存量缺口**：`docs/modules/MODULE_MAP.yaml` 164 条声明中 53 条路径不存在；`lib/` 代码注释 40 条引用不存在路径（真缺口 28 / 假阳性 12）。缺口清单与归属见差距清单登记。
25. **严格资源门旗标不可达**：`--resource-detail` 未登记进命令树 allowed 列表，CLI 以 `rc=2 unknown flag` 拒绝；`--strict-resource-gate` / `--on-resource-gate` 已登记进命令树，资源门已收窄为磁盘门（record-only），不改变运行判定。

## D. 未覆盖范围（产品面）

26. **多波长 / 多目标产品化**：本版成品仅 R 通道；G/B/H-alpha 与多目标批处理未做。
27. **运动目标 / 光谱 / 时序**：小行星、彗星等运动目标与光谱、时序产品不在本版范围。
28. **异常亮斑与 dense-field**：异常亮斑处理与高密度星场（dense-field）未覆盖。
29. **平台与形态**：Windows 腿、安装器、GUI/HiPS Browser、GPU/ACR 生产化、ARM 均不在本版范围。
