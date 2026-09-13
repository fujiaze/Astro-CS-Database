# M7 · I_DOC_HYGIENE（文档卫生）· P1

> **档位声明**：本文件内全部条目的 **类别与优先级由本行标题承载**（协议 §3 的「一类别×优先级一档」）；条目正文只在**偏离本档级别**时显式标注改档及理由（如 M7-A-101 记 P0→P1、M7-A-201 记 P1→P2 并撤核心结论、M7-A-001 记 P1→P0、M7-I-202 记 P1→P2 且改类 A_SCI_DEF→I_DOC_HYGIENE）。逐条四态判定与编号映射见 `问题扫描/_merge/M7.md` §2；每条均含 位置/权威依据/证据或证据出处/问题说明/影响/建议处置/置信度/related/四态判定 九项。

## M7-I-101 复杂度/内存声明与本文自身伪代码、数据结构不相容（跨 6 篇 ALG 的 8 处实例，L20-004）
- 类别: I_DOC_HYGIENE · 优先级: P1
- 位置（均为 `path::节` 符号锚；行号注「复核时」）
  - `docs/algorithms/UPM_SOLVER.md::§6 复杂度`（复核时 :56）对照 `PHASE2_UPM_IMPL.md::§9 内存`（:290-291）
  - `docs/algorithms/CALIBRATION_ALGORITHMS.md::§5`（:234-235 漏 max_iter；:237-239 IDW 分支最坏 O(npix²)）
  - `docs/algorithms/COSMETIC_ALGORITHMS.md::§6`（:196「总计 O(n)」与 :82 + DISP-COS-006 :285「无上限防护」互斥）
  - `docs/algorithms/PHASE2_COVERAGE.md::§6`（:163-164 内存 O(max|MOC_f|+K) 与 :2 逐帧 append→sort→unique 的 Σ|MOC_f| 前缓冲不符）
  - `docs/algorithms/DRIZZLE_GEOMETRY.md::§7`（:148-149「禁 per-leaf 全局 map」与 :132-133「per-thread unordered_map」互斥）
  - `docs/algorithms/NOISE_ESTIMATION.md::§6`（:59 把 patch 网格边长当矩阵规模）
  - `docs/algorithms/PLATESOLVE.md::§6`（:66 O(n log n) 只可能是 KD 查询项；三角形投票至少 O(n²)；n_triangles 全文未定义）
- 问题说明 ALG 是本仓复杂度的唯一权威层（宪章 §1.1 第 3 层）。上述声明或漏掉本文自己伪代码的主项（F×CG 迭代、materialize、max_iter）、或与同篇另一节直接矛盾、或把「每线程 O(X)」的常数界当成全局界、或量纲用错 ⇒ 资源门禁（宪章 §10.5/§17.6、§10.3 profiling 基准）失去可比基准；coverage 一条使真实峰值内存被低估 N 倍（N=参与帧数）。
- 影响 内存/CPU 门的判定、benchmark 选点、容量告警阈值均以这些声明为输入；「无上限防护」三处（COSMETIC/COVERAGE/整图缓冲）与宪章 §7.3 的禁无界缓存直接冲突，却被文档写成已界定。
- 建议处置 按各篇伪代码逐条重推上界（含迭代次数与每帧前缓冲）；三处「无上限」补绝对上限 + 显式拒绝路径；统一「每线程 O(X)」记法；DRIZZLE §7 与 §6 取一致者并登记 DISP。
- 置信度 高（8 处均为同篇两文对撞，无需外部信息）
- related L04-013（累加器字段数三处互斥，同数据结构主题）、L08-012、L09-010、M7-A-139（Phase3 全图平面缓冲）、宪章 §17.6/§7.3
- 四态判定 **仍成立**（8 处文本复核时全部在位）
