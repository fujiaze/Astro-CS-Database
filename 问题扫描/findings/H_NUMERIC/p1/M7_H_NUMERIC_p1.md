# M7 · H_NUMERIC（数值与收敛）· P1

## M7-H-101 迭代类算法「未收敛」在 6 篇 ALG 中三口径：无返回码 / 有码仍回填最优 / 收敛门量纲与产品精度脱钩（L20-003）
- 位置 `docs/algorithms/PHASE2_UPM_IMPL.md::§13 默认值表 + §6 F3 + §10 rc 表`（复核时 :380 命中 CG 判据）、`UPM_SOLVER.md::§4/§12`、`PHOTOMETRIC_FIT.md::§3/§4/§9`、`PLATESOLVE.md::§3`、`STAR_PSF_ALGORITHMS.md::§3`、`PHASE2_SAMPLER.md::§5.3 Stage B`（未定义符号 `s1`）
- 问题说明 ①UPM 的 rc 值域只有 0/1/2 ⇒ IRLS 跑满 `max_iterations=100` 与提前收敛**输出不可区分**，hash/persist 照常产出；②STAR_PSF 有 ITERATION_LIMIT=3 码却在单星接口仍回填最优参数，与同文档批接口的 NaN 语义相反，而其 §11.2 自称「四码语义冻结，无含糊」；③`tolerance=1e-6 ADU` 的绝对门远低于 C 场最终写入的 float32 tile 可表示精度 ⇒ 等价于恒跑满；④`PHASE2_SAMPLER §5.3` 伪代码引用从未赋值的 `s1`（同段变量名 `s0`）⇒ 不可执行。
- **本代理读码改判的一子项（原④）**：「跨 tile 邻接不对称 ⇒ (W+λsL+λ0I) 非 SPD、CG 前提不成立」**不成立**——`lib/phase2/src/upm.cpp::p2_upm_build_geo`（复核时 :352-358 `add_edge` 双向入边、:370-373 tile 内 4 邻、:376-413 跨 tile 按 `角距<1.6×cell_dist` 双向加边并于 :409-412 排序去重）证明邻接矩阵**对称** ⇒ L20 §6-R3 就此关闭；但对称≠正定：λs>0 时 L 半正定、其零空间含全局常数向量，仍依赖 `zero_anchor_weight` 或分量 gauge 消除，而生产侧该值为 0（L08-002 在册）⇒ **CG 前提改述为「无零锚时依赖 gauge 约束，SPD 未论证」而非「不对称」**。
- 影响 未收敛模型以成功态进入 mosaic 与持久化绑定；同一星在两接口给不同参数。
- 处置 「迭代耗尽」统一为显式可观测态（rc 位段或 Model.converged 字段且不入 hash）；收敛门改相对式并声明比较量单位；单星/批接口未收敛语义并轨；订正 `s1`；SPD 前提在 §12 补 gauge 条件。
- related L08-002（弱零锚生产为 0，本条与它构成闭合）、种子 L07-010、L06-011、M7-A-001（gauge/零锚同族）；判定 **部分成立**（①②③⑤成立；④的「不对称」被本代理读码否证、改述为「SPD 未论证」）；置信度 高

## M7-H-102 Drizzle 面积函数以 NAN 作半球哨兵，下游拒绝判据是 w≤0 ⇒ NaN 静默进累加器污染整 tile（L20-005）
- 位置 `docs/algorithms/DRIZZLE_GEOMETRY.md::§/compute_overlap_area_g`（复核时 :73/:121 命中 NAN）
- 问题说明 跨半球/退化多边形以 NAN 返回，而累加侧的跳过判据是 `w<=0`（NaN 与任何比较皆 false）⇒ NaN 通过判据进累加 ⇒ 整 tile 被 NaN 污染；属「哨兵值与判据不匹配」族（与 M7-H-103 的 0.0 哨兵 fail-open 同一失败模式、不同事实）。
- 处置 面积函数返回码与哨兵分列（或返回 −1 让 `w<=0` 能拦）；累加侧统一 `isfinite && >0` 双条件 + 显式计数。
- related L02-008、M2a 的 A11/DISP-DRZ-004（值像素 NaN 静默 continue 无计数，注册表 :163 已登记）、宪章 §7.3:224；判定 **仍成立**；置信度 高（可达性属 L20 §6-R7 待执行）

## M7-H-103 绝对量纲阈与哨兵跨 6 篇未随量纲/电平定标；UPM 求值接口双哨兵（NaN vs 0.0）构成 fail-open（L20-018）
- 位置 `docs/algorithms/PHASE2_UPM_IMPL.md::§6 F4/§13`（复核时 :173/:174/:176/:237 命中）、`PHASE3_RSMP_IMPL.md::§6.5`、`NOISE_ESTIMATION.md`、`DRIZZLE_GEOMETRY.md`
- 问题说明 ①求值接口对「越界/无覆盖」在两处分别返回 NaN 与 0.0，而下游把 0.0 当合法校正值 ⇒ `calibrated = raw − 0` 静默不校正（fail-open）；②`1e-20/1e-15` 等阈未写单位（sr² 还是 像素²），换单位即漂移（R8 待核）；③与 M7-H-101③ 同为「绝对阈不随电平定标」族。
- 处置 单一哨兵（推荐 NaN + 显式 rc 双出）+ 每处阈注单位；对 `calibrated` 加「校正量为哨兵即整像素 INVALID」断言。
- related L07-010、L02-008、M7-A-103、M7-H-102；判定 **仍成立**；置信度 高

## M7-H-104 噪声模型把负预测方差夹到 1e-12 地板 ⇒ 模型失效像素获得全链最大 ivar，且该地板被逐位冻结（L20-027）
- 位置 `docs/algorithms/NOISE_ESTIMATION.md::§/floor 块`（复核时 :11/:21/:26/:39）
- 问题说明 方差预测为负是模型失效的确证信号，实现却把它当「小数方差」抬到地板 ⇒ ivar=1e12 成为该像素在全链上的最大权重；且 §/测试面把 1e-12 逐位钉死 ⇒ 无人能改。与 SCI-NOISE 侧的三套互斥（M7-A-103）同量不同事实。
- 处置 负方差 ⇒ ivar=0 + 计数 + 状态位；地板只作用於「正且小」分支；把地板值纳入 M7-G-103 的四元常数表。
- related M7-A-103、M7-A-211、L07-009、L07-019、A-11（ivar==0 单裁语义）、M2a-H-2；判定 **仍成立**；置信度 高
