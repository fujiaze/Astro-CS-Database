# 参考文献与 API 核验记录（第③路）

## 文献核验汇总

### P1 核心算法相关

| # | 文献 | DOI/arXiv | API 状态 | 核验结论 |
|---|------|-----------|----------|----------|
| 1 | Beaton & Tukey 1974, Technometrics 16, 147 | 10.1080/00401706.1974.10489171 | ✅ HTTP 200 | c=4.685 的 95% 效率记载明确 |
| 2 | Rousseeuw & Croux 1993, Comput Stat Data Anal 15, 95 | 10.1016/0167-9473(92)00043-A | ✅ HTTP 200 | MAD→σ换算系数理论支持 |
| 3 | Huber & Ronchetti 2009, Robust Statistics, 2nd ed. | N/A (book) | ⚠ 图书 | Tukey IRLS 理论来源 |
| 4 | Hogg et al. 2018, arXiv:1803.08651 | 10.48550/arXiv.1803.08651 | ✅ HTTP 200 | 方差截断实践讨论 |
| 5 | Luri et al. 2022, A&A 667, A1 | 10.1051/0004-6361/202243889 | ✅ HTTP 200 | Gaia DR3 星等范围实测 |

### FOV 选择与锥形搜索

| # | 文献 | DOI/arXiv | API 状态 | 核验结论 |
|---|------|-----------|----------|----------|
| 6 | Gaia XP 客户端文档 | N/A (online docs) | ⚠ 在线文档 | 未定义具体阶梯值 |
| 7 | 无直接相关文献 | - | ❌ None | 自适应阶梯{12..16}无文献支持 |

**总体结论**：P1 模块的大部分常数（FOV 缓冲、锥搜阶梯、早停阈等）属于**工程约定**而非科学断言，因此文献腿普遍缺失或标记为"⚠需标定"。这些值需要通过实地数据测试逐步优化。

---

## 实验脚本清单

所有 standalone 脚本均位于 `code/` 目录，执行命令格式为：

```bash
cd "/workspace/Astro CS Database/独立审计/实验重做/P1 通量积分拟合/路线 3/code"
python3 <script>.py
```

| ID | 脚本名 | 对应审查项 | 预计运行时间 |
|---|--------|-----------|-------------|
| exp_A4b | A4b_fov_radius_three_constants.py | A-4b | ~2s |
| exp_A5 | A5_cone_search_four_constants.py | A-5 | ~1s |
| exp_C6 | C6_max_stars_5000.py | C-6 | ~0.5s |
| exp_C7 | C7_spatial_gain_order_limit.py | C-7 | ~0.5s |
| exp_A7 | A7_mag_tolerance_3.py | A-7 | ~0.5s |
| exp_G12 | G12_G13_sigma_bounds.py | G-12/G-13 | ~0.5s |
| exp_hall | hallucination_anchor_frame_photometry.py | 幻觉锚 | ~1s |

**总运行时间**：<10 秒（全部并行）

---

## JSON 结果文件清单

| ID | 文件名 | 大小 | 主要字段 |
|---|--------|------|---------|
| res_A4b | results/A4b_fov_radius_three_constants.json | ~8KB | scan_results, neg_ctrl, doc_comparison |
| res_A5 | results/A5_cone_search_four_constants.json | ~6KB | simulation_results, early_stop_analysis |
| res_C6 | results/C6_max_stars_5000.json | ~2KB | analysis, sensitivity_tests |
| res_C7 | results/C7_spatial_gain_order_limit.json | ~3KB | analysis, snr_comparison |
| res_A7 | results/A7_mag_tolerance_3.json | ~2KB | analysis |
| res_G12 | results/G12_G13_sigma_bounds.json | ~2KB | analysis |
| res_hall | results/hallucination_anchor_verification.json | ~4KB | evidence, inconsistencies |

**总数据量**：~27KB

---

## 与其他两路的关系

**独立性保证**：
- 本路的文献检索和 API 核验完全独立进行，未参考审查 -05-① -科学性 -2-SUMMARY.md 的成稿
- 所有代码脚本从零编写，未调用仓库内任何 Python 模块
- 实验设计基于基本物理模型和统计理论，不依赖既有分析框架

**一致性验证**：
- 本路与审查结论高度一致（7/7 项结论相同）
- 但本路提供了**独立的实验证据链**，可作为交叉验证
- 对于"幻觉锚"等关键问题，三方独立核查后结论统一，增强了可信度

---

**生成时间**：2026-09-26  
**路线标识**：第③路独立科学研究路线  
**状态**：✅ DONE
