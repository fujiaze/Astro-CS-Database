# AstroCS 已知限制（V19R8 — 延续 V19R2，补 V19R8 节点 76/95 B4 28/28 DONE）

1. **真实数据域**: V19R2 只做工程/合成验收; BASS + 2×2 + 3×3 + 大规模真实
   数据留下一轮 (`FINAL_REAL_DATA_VALIDATION=PENDING`)
2. **T1 数据集**: 无真实数据 (`WAITING_T1`, 外部阻塞, Phase1 冻结前即记录)
3. **Drizzle 相邻协方差**: pixfrac/resampling 引入, 已量化 (SNR-012) 未建模
4. **HISS**: deprecated, 不携带 variance 产品
5. **ACR**: dormant 基座, V19 未进入科学路径
6. **Sanitizer**: MSYS2 MinGW 无 ASan 运行库; WSL gcc 15 可跑（矩阵见
   reports/v19r2/sanitizer_matrix.md）
7. **Aladin GUI smoke**: 无 GUI 环境, 未执行
8. **weight/rejection_count 诊断产品**: JSON 诊断, 未输出为 Image HiPS
9. **UPM ivar 回退**: 输入帧无 ivar 产品时积分权重回退 support
   (`ivar_product_missing` 计数如实记录)
10. **Phase1 SNR catalogue**: 保留为 legacy 诊断, 不再作为 science weight
