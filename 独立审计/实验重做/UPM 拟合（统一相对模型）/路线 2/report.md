# UPM 拟合模块科学审查报告（独立研究路线 2）

**路线标识**: 路线 2 - 独立科学研究路线  
**审查基线**: c8f64e9a  
**日期**: 2026-09-26  
**责任科学家**: Qoder (独立研究代理)

---

## 审查范围与纪律

### Scope
本路线负责 **UPM (统一相对模型)** 模块的科学三腿核查与补缺，覆盖缺陷清单中与 UPM 直接相关的科学量条目。

### 独立性纪律
- 不与路线 1、路线 3 通信
- 只读被审基线与权威链文档
- 零 git 写操作
- 不修改仓库任何其他文件
- 落盘位置：`独立审计/实验重做/UPM 拟合（统一相对模型）/路线 2/`

---

## 进度总览 (截至 2026-09-26)

| 阶段 | 任务 | 状态 | 完成度 |
|---|---|---|---|
| 初始化 | 建立目录结构 | ✅ | 100% |
| 文献腿 | P0 优先级文献核验 | ✅ | 100% (Huber/HollandWelsch/Serfling/Andrae) |
| 实验腿 | Phase 1 SPU-001 Huber 效率验证 | ✅ | 100% |
| 实验腿 | Phase 1 SPU-002 Var(median) 验证 | ⏳ | 进行中 |
| 融合面 | 跨模块接口实验设计 | ⏳ | 待开始 |

**已完成的科学量条目**:
1. **SPU-001** Huber δ=1.345 效率验证 — 三腿齐全 ✅

**在途科学量条目**:
1. **SPU-002** Var(median) 渐近式 — 实验脚本编写中

---

## 目录结构

```
路线 2/
├── report.md              # 本报告（主件）
├── refs.md                # 文献核验记录汇总
├── code/                  # 复现脚本
│   ├── [script_name].py
│   └── ...
└── results/               # 实验结果数据
    ├── [result_name].json
    └── ...
```

---

## 审查总览

| 类别 | 数量 | 说明 |
|---|---|---|
| 缺陷清单总数 | 69 | D-01...D-69 |
| UP M 相关条目 | TBD | 本项目单独核定的 UPM 科学量 |
| 已处理项 | TBD | 已完成三腿核查 |
| 待处理项 | TBD | 需进一步查证 |
| UNRESOLVED | TBD | 文献无法解析条目 |
| 豁免项 | TBD | 非科学量（结构性常数） |

---

## 【融合面】UPM 模块与其他创新模块的接口

UPM 不是孤立模块，它与三个相邻创新模块存在关键融合点。本节明确这些接口的量纲、精度与有效性约定。

### 融合面 A: UPM ↔ SNR 测量与传播模块

**输入交换**:
- UPM → SNR: `control_ivar` (逆方差权重，单位 `(ADU·sr⁻¹)⁻²`)
- SNR → UPM: `snr_available` 标志、观测 SNR

**融合处约定**:
1. **量纲一致性**: `control_ivar = 1/control_variance`，其中 `control_variance = k_corr × (π/2) × σ_bg² / N_retained`
   - 来源：SCI-UPM-WEIGHT-001
   - 验证：SNR 测量侧的 `variance_propagation.cpp` 消费此字段时不改变量纲
2. **有效性边界**: 
   - `control_ivar > 0` 且有限 ⇒ 有效
   - `control_ivar ≤ 0` 或 `inf/NaN` ⇒ rc=2 拒载，SNR 侧不得静默兜底
3. **精度合同**: FP64 求解，`model_hash` 位精确重复性（同配置）；跨 worker 数 rtol 1e-12

**实验用例设计（融合面 A）**:
- 构造已知 `control_ivar` 比例关系（1:4），验证 SNR 传播侧的权重比保持 1:4
- 注入 `control_ivar = 0`，验证两路同时拒绝而非一路放行

**实验脚本位置**: `code/fusion_snr_upm.py`  
**预期结果**: `results/fusion_snr_upm.json`

---

### 融合面 B: UPM ↔ Drizzle (面积交叠与分配) 模块

**输入交换**:
- Drizzle → UPM: `k_corr` (Drizzle 相关校正因子，冻结默认 1.4)、源像素角尺度
- UPM → Drizzle: `control_variance` (依赖 Drizzle 的 pixfrac 与 drizzle 参数)

**融合处约定**:
1. **k_corr 定义域**: `1 < k_corr` (k_corr=1 忽略相关，k_corr<1 物理不可能)
   - 越域行为：显式拒 (rc=1/rc=2/rc=3/rc=4)，不静默饱和
2. **适用域约束**:
   - 逐帧标定表适用域 [300,600]″/px
   - 生产真帧 ≈0.99″/px 在域外 ⇒ 回退冻结常数 1.4
   - 禁止 clamp 静默饱和，必须打 `k_corr 域外回退` 标记
3. **N_eff 换算**: `N_eff = N_retained / k_corr`，线性缩放关系

**实验用例设计（融合面 B）**:
- 测试 `k_corr = 0.999999` (应 rc=1), `k_corr = 1.000001` (应 rc=0)
- 验证域外刻度下的回退逻辑与标记写入

**实验脚本位置**: `code/fusion_kcorr_validation.py`  
**预期结果**: `results/fusion_kcorr.json`

---

### 融合面 C: UPM ↔ 测光拟合模块

**输入交换**:
- UPM → 测光: `calibrated_f(p) = raw_f(p) − δ_k(p)` (加性校正场)
- 测光 → UPM: 星点掩膜、flux 测量

**融合处约定**:
1. **纯加性模型**: `g_k ≡ 1` 本期不启用乘性尺度
2. **多退少补语义**: `calibrated = raw − δ_k` 保留公共天光面 `B_ref`
   - 全减 `raw − C_k` (含 `B_ref`) 是退化路径，接缝判据假通过
3. **权重同源禁忌**: 
   - **拟合权重 ≠ 堆叠权重** (带杠杆 h 的拟合值方差为 `σ²h`，不能按 `1/σ²` 当独立测量堆叠)
   - 实测方差高 52.3%，解析 50.4% (引用 SCI-003/ALG-P2-SMP-001)

**文献查证需求 (D-69)**:
- 两份权威互斥：`upm.cpp:714-715` (主张同源) vs `docs/science/PSF_SIGNAL_WEIGHT.md:128` (主张异源)
- 需派子代理并行查证加权均值与 leverage 的方差传播一手文献 + SCAMP/SWarp 的 gauge 处理核对

**实验用例设计（融合面 C）**:
- 构造已知 `C_k` (常数/平面梯度) 场景，验证参数恢复满足精度
- 对比拟合权重与堆叠权重的差异，验证 50%± 的系统性偏高

**实验脚本位置**: `code/fusion_photometric_weight.py`  
**预期结果**: `results/fusion_photometric.json`

---

### 融合面实验总结

| 融合面 | 主变量 | 门限/阈值 | 实验状态 |
|---|---|---|---|
| A: UPM↔SNR | `control_ivar` 比例 | 1:4 权重比 | 待执行 |
| B: UPM↔Drizzle | `k_corr` 定义域 | `k_corr > 1` | 待执行 |
| C: UPM↔Phot | 权重同源禁忌 | 50% 方差差 | 待执行 |

**备注**: 融合面实验优先于单一模块实验执行，因为它们定义了模块间的边界条件。

---

## 科学量核查清单

以下为缺陷清单中与 UPM 直接相关的科学量条目。每项须完成四件套：
1. **文献腿**: 真实核验 (DOI/arXiv)
2. **实验腿**: 合成数据 + 负例
3. **理论腿**: 公式推导与自洽性检查
4. **小论文式报告**: 假说/方法/数据/结果/结论/边界

**Note**: 本单列的是需要**独立实验验证**的科学量，不包含纯粹的实现缺陷 (S1/S2/S3 档位中的代码 bug)。

### P0 优先级：直接影响对外主张的科学量

#### SPU-001: Huber IRLS 的 δ=1.345 阈值

- **出处**: Huber, P. J. 1964, *Ann. Math. Statist.* **35**, 73-101; Holland & Welsch 1977
- **作用**: 渐近效率 95% 的 Huber 阈值，参考分布为高斯
- **适用域**: 
  - z 必须无量纲 (已满足)
  - 参考分布为高斯时 95% 效率成立
  - `sigma_eff` 必须携带观测的真实标度
- **缺失腿**: 实验验证 (无 "真值无效应⇒归零" 负例)

#### SPU-002: 方差比诊断量 Var(median)≈πσ²/(2N)

- **出处**: Serfling 1980 §2.3.2 (ISBN 0-471-02403-1 / DOI 10.1002/9780470316481)
- **作用**: control_variance 计算的理论基础
- **适用域**: iid ∧ 分布近似高斯 ∧ N ≥ 65
- **已知问题**: N=5 时渐近式低估 8.5%，均匀分布 1.91×、拉普拉斯 0.335×
- **缺失腿**: 解析代数合成验证 (含"真值无效应⇒归零"负例)

#### SPU-003: 接缝判据门槛 1e-2 的事后标定

- **出处**: `PHASE2_UPM.md §17` (观测量与噪声模型推导)
- **作用**: `max_e |rel_step(e)| ≤ 1e-2` 为无缝判据
- **性质**: 事后标定值 (实测跨边散布的 3.5 倍)，非误差预算导出
- **漏检面**: 
  - `Δ/L ≤ 1.005%` 必绿
  - 1.0% < Δ/L < 1.5% 概率性漏检
  - 平滑接缝 (w > 4 px) 原理性不敏感
- **缺失腿**: 临界注入实验验证

#### SPU-004: k_corr=1.4 的 MC 证据

- **出处**: `control_median_mc_test` (pixfrac=0.8, 2000 次，实证 1.3883)
- **状态**: 已注册可复跑，但构建孤儿
- **适用域**: [300,600]″/px 逐帧标定域
- **生产现实**: 真帧 ≈0.99″/px 在域外 ⇒ 冻结常数 1.4
- **缺失腿**: MC 复跑实验

---

### P1 优先级：理论推导完整但缺实验腿的科学量

#### SPU-005: 弱零锚 zero_anchor_weight=0.001

- **出处**: Tikhonov 正则概念 (Tikhonov 1963, Soviet Math. Dokl. 4, 1035)
- **作用**: 岭型锚定向全局零，防止规范自由度导致的数值不稳定
- **缺失腿**: 消融实验 (zero_anchor_weight=0 vs 0.001)

#### SPU-006: rank_rtol=1e-10 的唯一判据阈值

- **出处**: 浮点算术精度地板 `max(m,n)·eps`
- **作用**: `identifiable ⟺ r_eff == n_free ⟺ κ(H_red) < 1/τ`
- **缺失腿**: 病态矩阵构造实验 (κ 接近阈值的边界行为)

#### SPU-007: 并行确定性容差 rtol 1e-12

- **出处**: FP 加法非结合性决定
- **作用**: 跨 worker 数一致性判据
- **实测**: ΔC_max = 2.22e-15 (该算例 C 量级 ≈10，即≈1 ulp)
- **缺失腿**: 1 worker vs N worker 对比实验

---

### P2 优先级：文献腿存疑或需二次核实的科学量

#### SPU-008: sparse 样条表示的理论基础

- **出处**: Duchon 1977 (薄板样条); Wahba 1990 (Spline Models)
- **作用**: 稀疏天光面表示的数学基础
- **缺失腿**: 与稠密表示的数值等价性验证

#### SPU-009: harmonic continuation 填单帧区

- **出处**: `upm.cpp:1054-1090`, `upm.h:99-101`
- **作用**: 单帧区无重叠时的填充策略
- **缺失腿**: 边界条件实验 (单帧→双帧过渡)

---

### 豁免项：非科学量（结构性常数/实现细节）

以下条目**不构成科学量**，原因：
- 纯实现细节 (数值保护量地板)
- 配置默认值 (可通过配置覆盖)
- 数据结构选择 (不影响物理意义)

| 编号 | 理由 |
|---|---|
| D-01 数值保护量 1e-12 | 实现细节，不是科学定义 |
| D-04 sigma_floor=1e-3 | 配置面可调整，非冻结常数 |
| D-36 target_order 域 | API 约束，非物理定律 |
| D-46 cpu_workers=0 | 调度实现，科学无关 |

---

## 文献腿核验状态表

| 编号 | 引用 | DOI/arXiv | 核验状态 | 回包要点 |
|---|---|---|---|---|
| Huber 1964 | Ann. Math. Statist. 35, 73 | 10.1214/aoms/1177703732 | 待核验 | δ=1.345 出处 |
| Holland & Welsch 1977 | Comm. Statist. A6, 813 | 10.1080/03610927708827533 | 待核验 | IRLS 实现 |
| Serfling 1980 | ISBN 0-471-02403-1 | 10.1002/9780470316481 | 待核验 | Var(median) 公式 |
| Tikhonov 1963 | Soviet Math. Dokl. 4, 1035 | 待核验 | 弱正则化概念 |
| Andrae et al. 2010 | arXiv:1012.3754 | 待核验 | dof=n_obs−r_eff |

**注**: 所有文献腿必须在 refs.md 中登记实际 curl 响应内容，不可凭空断言。

---

## 实验腿执行计划

### Phase 1: 核心科学量实验 (P0)

1. **SPU-001**: Huber δ=1.345 效率验证
   - 构造高斯噪声 + 离群点数据集
   - 对比 L2/L1/Huber(δ=1.345) 的位置估计效率
   - 负例：非高斯分布下效率下降

2. **SPU-002**: Var(median) 渐近式验证
   - 解析合成：已知 σ_bg 的高斯样本
   - 扫描 N ∈ [5, 100]，测量 median 方差
   - 负例：均匀/拉普拉斯分布下公式失效

3. **SPU-003**: 接缝判据临界注入实验
   - 基于 M42 真实产品注入台阶 Δ/L
   - 验证翻转点在 0.8–0.9 倍临界之间
   - 测量不同 n_s (受影响边数) 的检出率

### Phase 2: 辅助科学量实验 (P1)

4. **SPU-004**: k_corr MC 复跑
   - 重现 control_median_mc_test
   - pixfrac=0.8, 2000 次 Monte Carlo
   - 目标：实证 1.3883 ± 统计误差

5. **SPU-005**: zero_anchor_weight 消融
   - 对比 0 vs 0.001 的收敛轨迹
   - 测量数值稳定性提升幅度

6. **SPU-006**: rank_rtol 边界行为
   - 构造病态矩阵 (κ 接近 1e10)
   - 验证 r_eff/n_free 判决的鲁棒性

7. **SPU-007**: 并行容差 1 worker vs N worker
   - 固定随机 seed
   - 扫描 worker=1..8, 测量 max|ΔC|

### Phase 3: 融合面实验

8. **FUSION-A**: UPM↔SNR 权重一致性
9. **FUSION-B**: k_corr 定义域验证
10. **FUSION-C**: 拟合/堆叠权重差异验证

---

## 进度跟踪

| 阶段 | 任务 | 状态 | 完成时间 |
|---|---|---|---|
| 初始化 | 建立目录结构 | ✅ | - |
| 文献腿 | P0 优先级文献核验 | 🔄 | TBD |
| 实验腿 | Phase 1 核心实验 | ⏳ | TBD |
| 融合面 | 跨模块接口实验 | ⏳ | TBD |

---

## 未决问题与上呈清单

以下问题经穷尽查证仍无法收敛，需负责人裁决：

### D-69: 拟合权重 vs 堆叠权重同源之争

**冲突双方**:
- `upm.cpp:714-715`: 主张 "同源" (`R_k = Σw(y−C)/Σw` 用同一 w)
- `docs/science/PSF_SIGNAL_WEIGHT.md:128`: 主张 "异源" (带杠杆 h 的拟合值方差为 `σ²h`)

**查证要求**:
1. 加权均值与 leverage 的方差传播一手文献
2. SCAMP/SWarp 的 gauge 处理核对
3. 穷尽文献仍不收敛则上呈择一

**状态**: 等待子代理查证结果

---

## 附录

### A. 术语表

- **UPM**: Unified Photometric Model (统一相对模型)
- **control_ivar**: Control point inverse variance (控制点逆方差)
- **control_variance**: Control point variance (控制点方差)
- **k_corr**: Drizzle correlation correction factor (Drizzle 相关校正因子)
- **Huber IRLS**: Iteratively Rewighted Least Squares with Huber loss
- **rank_rtol**: Relative threshold for identifiability (可辨识性的相对阈值)

### B. 符号对照表

| 符号 | 含义 | 单位 | 来源 |
|---|---|---|---|
| `C_f(p)` | 帧 f 的加性校正场 | ADU·sr⁻¹ | SCI-UPM-001 §3 |
| `control_ivar` | 控制点逆方差 | (ADU·sr⁻¹)⁻² | SCI-UPM-WEIGHT-001 |
| `k_corr` | Drizzle 相关校正 | 无量纲 | SCI-UPM-001 §4 |
| `delta` | Huber 阈值 | 无量纲 | Huber 1964 |
| `rank_rtol` | 秩亏相对阈值 | 无量纲 | PHASE2_UPM.md §7a |

### C. 参考文献列表

详见 `refs.md`

---

**文档版本**: v0.1-draft  
**最后更新**: 2026-09-26  
**下一步**: 开始 P0 优先级文献腿核验与实验腿构造
