# STD-F1-ADJ｜CRPIX 口径落地（前台裁决 = 方案 b：导出边界显式 +1 桥接）

> **rev6 修订说明（前台裁决 R-02）**：本任务原为 `owner` lane 的 `BLOCKED_EXTERNAL`
> （"待负责人三选一 a/b/c"）。经依宪章 §7.3 + §1.1 裁决，**只有方案 (b) 为宪章可导出解**，
> 故转 `repo-write` / `SA-WCS` 正常派发执行；负责人的最终追认不阻塞执行（见 07 号 §3）。

## 目标
消除 `lib/plate_solve` 迭代反演输出口径与 FITS 消费方口径之间恒定的 **1px 系统偏移风险**，
并把该边界写成合同条款，使 Phase3 导出链与第三方工具（astropy 等）混用时不产生 1px 错位。

**现状（前台已核实）**：
- `docs/science/ASTROMETRY.md` 冻结 1-based：`xp = x+1`、`CRPIX=(w/2+0.5, h/2+0.5)`、`(u,v)=CD·(xp−CRPIX)+SIP`；
- `lib/phase3_session/p3_wcs.cpp:97-98` 消费方 `dx=(x+1)−crpix`（FITS Paper I §2.1.1 标准口径）；
- `lib/plate_solve` 迭代反演输出为 `u = x − crpix`（**0-based 语义**）——两者相差常量 1px。

**裁决内容（方案 b）**：
ipv 内部**保持**既有 0-based 自洽约定（其自身 roundtrip 与 astropy 在 `crpix+1` 桥接后均达机器精度，
证明是纯原点平移而非数学内容差），**在导出边界做显式 +1 桥接**，并把桥接责任方写成合同条款。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `lib/plate_solve/`
- `lib/phase3_session/`
- `docs/`
- `tests/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-02**：
  - 宪章 §7.3「Phase3 必须以 FITS WCS Paper I、Paper II、HEALPix 和 IVOA HiPS 规范为基础，
    **而不是根据现有代码反推科学定义**」；
  - 宪章 §1.1 权威分层：本文(1) > SCI(2) > ALG(3) > DATA(4) > ARCH(5) > API/ABI(6) > 代码/测试(7)，
    `p3_wcs.cpp` 属第 7 层；
  - FITS WCS Paper I §2.1.1 规定 `CRPIX` 为 **1-based** 参考像素。
- **否决 (a)**：把 ipv 内部改成 1-based 会改动 `ASTROMETRY.md` 冻结锚 F3 并连带全部既有对拍基线，
  属"因代码反推科学定义"，且触发宪章 §1.2 宪章变更流程 —— 前台无权执行。
- **否决 (c)**：「维持现状 + 合同标注双口径」不满足 §7.3 的"以标准为基础"，把 1px 风险留给消费方，
  违反 §14.4 fail-fast。

## 非目标与禁令
- **不改动冻结锚 F3 的数学内容**、不改 `CRPIX = w/2+0.5` 冻结不变量、不改冻结门 `1e-4px`、不动任何默认容差。
- 不顺手修复域外问题；发现后登记 finding（05 号登记册续写）。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。
- **严禁以"放宽断言 / 加入 known-failures 基线 / 改 waivable / 跳过测试"的方式让红灯消失**（裁决 R-05）。

## 必须动作
1. 读取宪章 §7.3、`docs/science/ASTROMETRY.md`、`docs/standards/STANDARDS_REGISTRY.md`（STD-F1 行）与 `lib/plate_solve` 实现；
2. **在导出边界实现显式 +1 桥接**（单一桥接点，形状与责任方明确；不得散落多处隐式 `+1`）；
3. **合同条款落地**：在 `docs/science/ASTROMETRY.md` 与 `STANDARDS_REGISTRY.md` STD-F1 行写明
   「ipv 内部 0-based / FITS 导出 1-based / 桥接点位置与责任方 / 冻结门 1e-4px 不变」，
   并把 STD-F1 行的符合性状态由 `OPEN（待负责人裁决）` 更新为明确结论；
4. **测试设计先行（先红后绿 + 负向注入必败）**：
   - `p1wcs` 全组 + `p3_wcs` 回归 + astropy 交叉全绿；
   - **负向注入**：移除/错置桥接 ⇒ 对拍必须 FAIL（证明桥接不是恒真装饰）；
   - 往返 Oracle：`‖(x,y) − WCS^{-1}(WCS(x,y))‖ < 1e-6 px`（FP64）不变量保持；
5. **九宫格视觉核验**：按负责人 rev3.1 口径（中心 1 格 + 四角 4 格 + 四边中点 4 格，9×100×100px）
   显式验证**无 1px 偏移**，产出可读预览与结构化结论；偏移 > 3/27 或 parity 翻转 = FINDING；
6. 命令证据（timeout/cwd/argv/起止/rc/stdout/stderr/SHA）落 `run/`；
7. 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 验收
- write_scope 零越界；预存 dirty 零覆盖；所有新测试故障注入必败。
- 无论实现细节，`p1wcs` 全组 + `p3_wcs` 回归 + astropy 交叉 + 九宫格全绿；冻结门 `1e-4px` 不变。
- `scientific_change` 必须显式声明：本任务为**口径边界**修复，**不改变任何科学公式与数值内容**。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 桥接点说明 + 九宫格预览与判定 + STD-F1 行更新前后对照。

## 关联 finding
STD-F1（原 WCS-003-F1）；G-STD 门禁依赖本任务；REAL-001 的 solve 链九宫格核验依赖本任务闭环。
