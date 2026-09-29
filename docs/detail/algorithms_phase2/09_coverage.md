# 插件文档：coverage（覆盖联合）

> 上游：ASTROCS_DESIGN.md §5.2（固定科学流程）

## 1. 职责与边界

- **职责**：建立输入帧/区域的重叠图与几何有效域，输出 coverage 产品与连通分量。
- **不是**：不做权重；**coverage 是几何/数据有效域，不是权重，不能作为 inverse-variance 或 SNR 权重**；不做集成。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §5.2（固定科学流程：coverage 重叠图）与 §5.6（coverage 不作权重）
- `docs/detail/PHASE2_DETAILED_DESIGN.md` §3
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（有效域语义）

## 3. 输入/输出数据合同

- **输入**：一组合同兼容 Phase1 产品（含各自 coverage/validity/WCS/manifest）；产品的落盘形态不进入科学语义（裸/归档同义，形态由落盘名判定）；`hips_paths` 的元素**保持字符串**，逐帧产品级索引路径由命名规则派生（`<name>.hips` / `<name>.hips.zst` → `<name>.hips.index.json`）；按天区查帧集合走**块级覆盖索引**（不压缩；由**加性可选键** `coverage_index` 引用数据集级 `coverage.index.json`，缺失时由产品级索引现场倒排），不逐瓦片探测。输入合同**不设** `storage_form` 键（阶段二产物固定裸形态），出现即 REJECT。
- **索引边界**：索引给出块 → 候选帧集合与覆盖分数，只用于剪枝与调度；像素级裁决仍由 support/validity/排异语义执行；**有效性来源 = support/validity/排异语义本身**。
- **输出**：帧/区域重叠图、有效面积、信息量、连通分量、coverage 产品。
- 参考：`eng/contracts/schemas/unified/coverage.schema.json`（coverage 对象 canonical schema）。

## 4. 算法与公式要点

- 重叠图：帧间球面交叠（几何有效域交集），以 MOC 表达（`Ω = MOC_1 ∪ … ∪ MOC_N`，NESTED）；定义、推导与适用域正本 = `docs/detail/PHASE2_DETAILED_DESIGN.md` §3 与 `docs/science/UNCERTAINTY_AND_COVARIANCE.md`（有效域语义）；
- 记录有效面积（球面交叠面积积分，单位 deg²/sr）与信息量（可推导到 `point_information` 的域面；该量的定义与推导正本 = `docs/science/PSF_SIGNAL_WEIGHT.md`，插件侧口径见 `docs/detail/algorithms_phase1/07_noise_snr.md` §4.6）；
- 连通分量：在几何有效域上按球面邻接求连通分量，互不相连的分量一律分组件输出，**不**按同一零点/背景基准合并；分量划分由 §5 的 `connected_components` 键控制。
- **假设与适用域**：输入帧的同一零点/背景基准只在**连通分量内**成立；几何有效域不重叠的帧不构成同一分量（域外不做外推）。验证方式见 §8（解析重叠几何 + 断图检测）。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `min_overlap` | —— | deg² | 最小有效重叠 |
| `connected_components` | true | —— | 是否分解连通分量 |

## 6. 接口/ABI

- entrypoint：Phase1 产品组 → 重叠图+coverage；模块落点 = `lib/algorithms/coverage/src/coverage.cpp`（签名头 `lib/algorithms/coverage/include/astro/phase2/coverage.h`，C ABI 入口 `p2_coverage_build`；模块注册 = `lib/infrastructure/pipeline/module_ports.registry.json` 的 `astrocs.phase2.coverage`）；生产接线 = `lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `p2_op_coverage`；
- 输出可被 sampling/upm/integration 消费。

## 7. 错误与边界

- 输入互不兼容（不同 frame/滤镜/单位）→ 拒绝；
- 断图 → 输出分组件，不假装同一基准；
- coverage 缺失的输入 → fail-closed。

## 8. 测试与 Oracle

- 构造已知重叠几何 → 覆盖面积/连通分量符合解析；
- 断图检测；
- coverage 不作为权重的负例测试。
