## M2a-A-1 SCI-DRZ-001 的 pixfrac 归一化与其自身"常数场流量守恒"不变量互斥：常数面亮度 B0 的输出为 B0/pixfrac²

- 类别: A_SCI_DEF
- 优先级: P0
- 来源: L04-002
- 复核时点结论: **仍成立**（推导逐行自证，代码按 SCI 公式实现，故缺陷在冻结合同本身）
- 位置: docs/science/DRIZZLE.md::§5 球面 drizzle 算法（权重/累加式）；docs/science/DRIZZLE.md::§7 物理不变量；docs/science/DRIZZLE.md::§11 验证 Oracle；docs/science/DRIZZLE.md::§2 符号表（pixfrac 行）；对照 lib/healpix_db/healpix_drizzle/drizzle_engine.cpp::processPixelSharedTiled（overlap_area/drop_area 段）、lib/astro_image_io/src/hips/aio_hips_writer.cpp::write_tile_core（sig = flux/area 段）、lib/healpix_db/healpix_drizzle/spherical_overlap.cpp::compute_drop_corners（half = 0.5·pixfrac）、lib/orchestrator/cpp/include/json_config.h::DrizzleConfig::pixfrac
- 证据摘录（逐字，复核时点现文）:
  > （DRIZZLE.md:48-51）w_jp = a_jp / A_drop,j ｜ F_p = Σ_j x_j·w_jp ｜ D_p = Σ_j a_jp ｜ S_p = F_p / D_p ｜ sumVarNum += Σ_j v_j·w_jp² ｜ variance_p = sumVarNum / D_p²
  > （DRIZZLE.md:82-84）每像素常量 ADU=C 时：S_p = Σ C·(a_jp/A_drop,j) / D_p = C/A_drop ≠ C——这是**正确的面亮度语义**（drizzle 输出为平均面亮度）…常数**面亮度** B0（ADU/sr）的 drop 满足 x_j=B0·A_drop,j ⇒ F_p=B0·D_p ⇒ S_p=B0（流量守恒）
  > （DRIZZLE.md:117）- **流量守恒**：常数场 `C` 的 `S_p=C` 全像素 `max_abs==0`。
  > （DRIZZLE.md:21,27）| `pixfrac p` | drop 收缩因子 (0,1] | 无量纲 | §4 §5 | …；`S,F,x`: ADU/e⁻；`D,a,A_drop`: px²（球面立体角等价）
  > （drizzle_engine.cpp:1508）Scalar weight = overlap_area / drop_area;  （:1530）acc.sumFlux += Scalar(pixelValue * weight);  （:1531）acc.sumArea += Scalar(overlap_area);
  > （aio_hips_writer.cpp:524-525）sig = flux / area;  sup = area / ps->A_cell;
  > （spherical_overlap.cpp:799 与 :910）T half = T(0.5) * pixfrac;   （drop 四角 = 源像素角点在**两个线性维**各乘 pixfrac）
  > （json_config.h:67）double pixfrac = 0.8;                // (0, 1]; 生产默认 0.8
- 权威依据: SCI-DRZ-001 §2/§5/§7/§11（docs/science/DRIZZLE.md，FROZEN）；宪章 §4.1（signal/variance/support/coverage 不得混同）、§5.3（Drizzle 须验证能量/面亮度语义）、§7.3（面亮度与每像素通量传播规则必须由 SCI/ALG 明确）、§12.3-3（公式唯一实现）、§13.1（性质测试）；Fruchter & Hook 2002, PASP 114, 157（drizzle 输出为面亮度重采样，drop 面积归一后须与 pixfrac 无关）
- 问题说明: drop 是源像素在**两个线性维**各缩 pixfrac 的结果（spherical_overlap 两处的 half=0.5·pixfrac 即其构造），故 `A_drop,j = pixfrac²·A_pixel_j`。把 §7 自己给的前提 `x_j = B0·A_pixel_j`（每像素通量 = 面亮度 × 该像素面积）代入 §5：`F_p = Σ B0·A_pixel_j·a_jp/(pixfrac²·A_pixel_j) = (B0/pixfrac²)·Σa_jp = (B0/pixfrac²)·D_p` ⇒ **`S_p = B0/pixfrac²`**。A_pixel_j 在每一项内被逐元约掉，因此该因子与网格、nside、覆盖完整度、单/多帧无关，只由 pixfrac 决定；`pixfrac=1` 时恰好等于 B0，`pixfrac<1` 时系统性偏亮 1/pixfrac²（0.8 → 1.5625×；0.5 → 4×）。于是 §11:117「常数场 C 的 S_p=C」与 §7:84「B0 ⇒ S_p=B0」在同一符号 C/B0 下与 §5+§7:82 的推导直接互斥——三者不可能同时为真。实现按 §5 公式逐字落地（:1508/:1530/:1531 → writer :524），所以这不是实现偏离合同，而是**冻结合同里的科学不变量写错**：正确的守恒式应是 `S_p = B0·(A_pixel/A_drop)·pixfrac²` 形式，或把归一分母改为 `D_p/pixfrac²`（等价于按 drop 面积而非覆盖面积归一）。
- 影响: 按 SCI §7/§11 编写的下游消费（面亮度产品标定、跨 pixfrac 配置对照、孔径测光流量闭合、Phase2 ivar 加权）在 pixfrac≠1 时系统性偏亮 1/pixfrac²；生产默认入口 json_config.h:67 正是 0.8（stage1.template.json:41 同值），而三套 gc_panel 配置取 1.0 → 同一条科学链上不同配置的产品不可直接比较。宪章 §17.1（SCI 已冻结且无冲突）与 §5.3 的能量/面亮度语义验证在当下不成立。
- 建议处置: ① 由科学 Owner 就 §5 的归一分母作唯一裁决（`D_p` 还是 `D_p/pixfrac²`），并同步订正 §7:82-84 与 §11:117 的常数量纲（写明 C 是"每像素 ADU"还是"面亮度 B0"）；② 在此之前**禁止**任何按 SCI §11 判 PASS 的面亮度声明；③ 订正 §2/§3 单位表使 A_drop 与 D 的单位与归一后的 S 单位闭合（见 M2a-A-2）；④ 派生回归门：同一常数场在 pixfrac∈{1.0,0.8,0.5} 下 S_p 恒定（当前注册面零覆盖，见 M2a-F-4）。
- 置信度: 高（推导为纯代数，双方锚文均逐字复读；生产默认值取代码现状）
- related: L04-010（单位链）、L04-011（pixfrac 默认值分裂）、L04-004/L04-015（证据面）、F00-06（跨域无涉）
