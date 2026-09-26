# 文献核验记录 - 结构文件

## 待核验项清单（来自审查意见）

### A-4b FOV 半径相关常数（缺三腿）
1. K-26: 缓冲值 `1.2` [deg]
2. K-27: 钳位下界 `1.0` [deg]
3. K-28: 钳位上界 `10.0` [deg]

### A-5 锥形搜索相关常数（缺三腿）
4. mag_max_arr = {12, 13, 14, 15, 16} [mag] - 5 个值
5. 早停阈 `2000` [stars]
6. 循环上限 `5` [iterations]

### A-8 IRLS/Tukey 收敛参数（已有锚，仅需复核）
7. `tolerance = 1e-6` [dex]
8. `max_iter = 50` [iterations]
9. `c = 4.685` (Tukey shape parameter)

### V-3 F_syn 谱网格参数（待建验证程序）
10. wl_start = 336 [nm]
11. wl_step = 2 [nm]
12. wl_count = 343 [points]

### 其他待核验项...

*每项格式：DOI/arXiv → 调用结果 → 结论*
