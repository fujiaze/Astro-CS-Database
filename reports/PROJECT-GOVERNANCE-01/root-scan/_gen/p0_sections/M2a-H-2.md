## M2a-H-2 variance≤0 的源像素被整颗丢弃（信号/覆盖/nContrib 一并丢失，无计数）：不确定度面被当 validity 掩膜，且该语义 SCI 未定义

- 类别: H_NUMERIC
- 优先级: P0
- 来源: L04-003
- 复核时点结论: **仍成立**（三处文本互斥逐条核对；实现与 DATA/ALG 只在 ≤0 半边一致，NaN 半边相反）
- 位置: lib/healpix_db/healpix_drizzle/drizzle_engine.cpp::drizzleTiled 主循环（variance/weight ≤0 continue 段）；::processPixelSharedTiled（varianceValue>0 累加门）；drizzle_engine.h::processPixel（varianceValue 形参注释）；docs/contracts/DATA_SEMANTICS.md::§11.1 variance 面行；docs/algorithms/DRIZZLE_GEOMETRY.md::§5 表（SNR/权重/variance 行）、::§10 DISP-DRZ 表；docs/science/DRIZZLE.md::§4 输入有效域、::§5、::§8
- 证据摘录（逐字，复核时点现文）:
  > （drizzle_engine.cpp:1733-1736）float varianceValue = 0.0f; if (varianceData) { varianceValue = varianceData[...]; if (varianceValue <= 0.0f) continue;  // 合法数据边界, 非掩膜 }
  > （drizzle_engine.cpp:1739-1740）nSourcePixels++;
 threadCounters[...].source_pixels++;   // 位于上述 continue 之后 → 被跳像素不计数
  > （drizzle_engine.cpp:1534-1538）if (varianceValue > 0.0f) { ... sumVarNum += ...; } ... acc.nContrib++;
  > （drizzle_engine.h:265）float varianceValue,             // 逐像素方差 (0 = 未知, 跳过传播)
  > （DATA_SEMANTICS.md:254）| variance 面（可选，帧内块） | float32，随 data 布局 | ADU² | 非有限或 ≤0 → 跳过该像素（:1727-1729）；无 variance 输入 → 不产 variance/ivar 产品 |
  > （DRIZZLE_GEOMETRY.md:119）| SNR/权重/variance 面非有限或 ≤0 | 静默跳过该像素 | :1718-1732 |
  > （docs/science/DRIZZLE.md §5 sumVarNum 行 / §8:96 值像素传播行）SCI 全文无"variance≤0 → 整颗像素不贡献"的任何条款；§4 输入有效域只列坐标/WCS/多通道/nside/pixfrac
- 权威依据: 宪章 §4.1（signal/variance/ivar/support/coverage/validity 不得混同，validity 是独立状态）、§7.3（不支持的数据语义必须显式拒绝，不得静默丢弃）、§5.3（不确定度传播须验证）；SCI-DRZ-001 §4/§5/§8；DATA-GAIA-001 无涉
- 问题说明: 三套口径互斥且实现只满足其中半套：**(a)** 头注释说 variance=0 只"跳过传播"（信号仍应进累加器）；**(b)** ALG §5/DATA §11.1 说"非有限或 ≤0 → 跳过该像素"；**(c)** 实现是「≤0 → `continue` 整颗像素（信号 F_p、覆盖 D_p、nContrib、nSourcePixels 全不含）」＋「NaN → 不满足 `<=0`，继续进主循环，只在 :1534 的 `varianceValue > 0.0f` 门里丢掉方差项」。所以：≤0 时实现违 (a)；NaN 时实现违 (b) 的"非有限→跳过该像素"；而 SCI 侧根本没有授权任何一种"用方差值决定像素有效性"的行为。后果最重的是 (c) 的 ≤0 分支：一颗有合法通量、只是噪声模型未给正方差（0/负值占位、blank-sky 边界、上游 `.std/fvar` 缺失回填 0）的像素，其**科学信号与覆盖面积同时消失且不计数**，等价于把 variance 面当掩膜使用——正是宪章 §4.1 点名禁止的混同；统计面（nSourcePixels/quick_rejects）亦随之失真，事后无法从计数发现丢了多少。
- 影响: 产品 signal/support/coverage 被不确定度面静默改写（丢像素贡献 → D_p 偏小 → 与 M2a-A-1 的归一偏差叠加）；ivar/方差产品覆盖面与 signal 覆盖面不再同源；§7.3"不得静默丢弃"与 §4.1 双违；无任何计数或 provenance 暴露，回归/审核不可见。
- 建议处置: ① 由 Owner 在 SCI §4/§5 明确 variance≤0 与非有限 variance 的语义（建议：只丢方差项、不丢信号与覆盖，与 :1534 门对齐），② 实现改为 `if (!(varianceValue > 0.0f)) { /* 只跳过方差累加 */ }` 并新增 `variance_skipped` 计数进 counters/p1_stack.json，③ 同步订正 drizzle_engine.h:265、ALG §5:119、DATA §11.1:254 三处文本为同一口径；④ 补注册面断言（p1drz 现有负面矩阵只覆盖 pixfrac/RING/多通道/缺 WCS/NaN 值面，无 variance≤0 用例）。
- 置信度: 高（四处代码现状与三处文本逐字复读；"合法数据边界, 非掩膜"注释与紧邻的整像素 continue 构成直接自证冲突）
- related: L04-005（同一处代码族的文档失实）、M2a-A-1（归一化叠加）、M2a-H-4（variance 提前降精度）、L04-004（无回归测试）
