# 工程控制 / RELEASE-01 阶段汇总（SUMMARY）

> FIN-001 完成后由前台填写，交付负责人。**当前状态：FIN-001 未启动**（前置条件未满足，见 §5/§6）。

## 1. 目标达成

| 控制包目标 | 实际完成 | 状态 |
|---|---|---|
| 文档包替换为最高权威 | 36 篇落位（23 同 / 12 更新 / 1 新增），非语义清理 3 处，引用可达核验通过 | 完成 |
| 设计-实现差异审计（GAP_AUDIT） | 4 分片只读审计，23 模块 + 横向全覆盖；P0=16、P1=87、P2=35、UNRESOLVED=11 主题 | 完成 |
| 科学自审与文献对照 | 研究包 7/7 + 11 主题对照；"我方有误" 5+34 项；45 篇补参考文献/参考代码库（文献 47 + 代码库 23） | 完成（P0 两项待变更 claim 裁决） |
| 逐模块并行构建 + 冒烟 | cmake/ninja rc=0（0 警告）；ctest 442 = 430 通过 / 12 跳过 / 0 失败 | 完成（机器门未全绿） |
| 测试审查与补充 | 缺口矩阵 + 跳过判定 + 断言强度抽检；新增负例测试 1 文件（10 负例）；暴露 fail-open 缺陷并修复 | 完成 |
| 其他文档自查 | 38 篇；6 处非语义订正；Mermaid 30/30 parse PASS | 完成 |
| 全流程真实数据端到端 | **L3** 两组三命令 rc 全 0；**L4 R 通道全量** 15 normalize（81 帧）+ 2 mosaic + 4 export 全 rc=0 | 完成（R 通道口径） |
| 视觉验证 | 整幅 + 4×4 分块 + 5 张裁剪放大；**判定 FAIL**（接缝/背景均匀不通过） | 完成（结论为不通过） |
| 性能计时与热点 | 分段计时 + 热点 + L2 复算 + 记录点现状；**L2 enforce 非零违约** | 完成（结论为违约） |
| 成品帧交付 | 两个 R 通道平面 FITS（M42 7821×10947 / GC 4856×9160，2 HDU） | 完成（待负责人检查） |
| 收尾与发布准备 | — | **未启动**（需负责人认可 DEL-001 + 裁决 §6） |

## 2. 差距闭合

- **P0（16 条）**：0 条闭合。其中 P0-01（CHK-MODULE-MANIFEST）为负责人已裁决的临时红；P0-02/03/05/06/07/11/12/13 为代码侧缺口；P0-08/09/10 为新文档包新增语义未实现；P0-14/15/16 为文档↔仓库口径冲突。
- **P1（87 条）**：本轮修复 1 条（`ci/verify_toolchain.py` fail-open，TST-001 发现 → 前台修复 + 负例锁定）；其余登记待分派。
- **P2（35 条）**：登记遗留。
- **UNRESOLVED（11 主题）**：全部上呈负责人（GAP_AUDIT §5）。

## 3. 科学自审结论

- **SNR/PSFSW 专项（S1）**：PixInsight 五节 .pidoc + PCL 2.10.4 精读；8 个开源实现源码级核验；photutils 3.0.0 数值对拍通过。发现"我方有误"5 项（PSFSNR 公式应为 `c3(Σf)²/(c4σ_n²)`；`psf_snr_power` 实为 DEFERRED；研究包混引 ZOGY 与 How-to-COAAD-I；DSS 许可证/URL；SEP 许可证）。
- **11 主题（S2）**：34 项"我方有误/需订正"（P0 2：DRIZZLE 归一化 `S=B0/pixfrac²` 与 §7 不变量互斥、ASTROMETRY §5a 的 1px 平移口径与 Paper I 及实现矛盾）。
- **新增参考文献与参考代码库**：文献 47 条、参考代码库 23 项（含许可证），落位 `docs/references/SCIENTIFIC_REFERENCES.md` 与各科学文档参考节；`tools/science_contract_lint.py` 对 10 篇 15 节 SCI 文档 PASS。
- **与负责人设计意图的差异**：负责人新文档包中若干新增方法学（稀疏天光面、PSFSNR/PSFSW 语义、权重词表）与**已冻结科学文档/现有实现**存在实质冲突，已在 GAP_AUDIT §5 逐条登记，不自行裁决。

## 4. 模块状态

| 面 | 模块数 | CONTRACT_READY | IMPLEMENTED | INSTALLED | VERIFIED |
|---|---|---|---|---|---|
| normalize（algorithms_phase1） | 8 | 8 | 部分（见 AUD-A1） | 部分 | 否 |
| mosaic（algorithms_phase2） | 5 | 5 | 部分（见 AUD-A2） | 部分 | 否 |
| export（algorithms_phase3） | 3 | 3 | 部分（见 AUD-A3） | 部分 | 否 |
| infrastructure | 7 | 7 | 部分（见 AUD-A4） | 部分 | 否 |

说明：机读模块门 `CHK-MODULE-MANIFEST` 报 139 findings / 20 模块 NOT_IMPLEMENTED；本表"部分"即以该门与分片审计为准，不冒充完成。

## 5. 验收结论

- **L1 合成科学**：ctest 442 全绿（含各模块 Oracle/不变量），但 TST-001 指出 6 个 C++ 空/恒真断言与 3 个未链产品库的"假测试"，且 ISA 等价/双平台误差在模块级空白 → **有条件通过（待补强）**。
- **L2 合成性能 / 真实性能**：**不通过**。PERF-001 复算 G-RES-01：判据③ 4/4 normalize 违约、判据① 3/4 mosaic 违约；mosaic 近单线程（利用率 5.6–6.4%，io_wait 最高 94.47%）；Phase1 峰值 RSS 无界增长（斜率 45.1–77.5 MiB/s，回收 <0.5）；4 项编排/缓存指标无记录点。
- **L3 小批量真实数据端到端**：**通过**（两组 normalize→mosaic→export 全 rc=0；`weight_mode=2` 负例 rc=2 fail-closed 正确）。
- **L4 全量真实数据 + 视觉**：产品链 **通过**（R 通道全量 rc=0，2 HDU 结构完整）；**视觉判定 FAIL**（接缝/背景均匀不通过）。
- **机器门**：`ci/run_checks.py` 全量未复跑（build 类重）；已知红：CHK-MODULE-MANIFEST（负责人已裁决）、CHK-REGISTRY-DOC-SYNC（U-2）、ENG-CONSTRAINTS（U-1）、`tests/quality/test_root_cleanliness.py`（U-1 派生）。

## 6. 遗留项与发布建议

- **遗留**：P0 16 条全部 OPEN；P1 86 条待分派；P2 35 条；UNRESOLVED 11 主题；Windows 平台本轮未构建/未验收（如实声明）。
- **需负责人先行裁决的三项**（决定结论走向）：
  1. 文档包 §7 / §2 两处删除是否回写（U-1/U-2）；
  2. 版本口径唯一化（U-3/U-F，FIN-001 前置）；
  3. **范围裁决**：新文档包新增语义（稀疏天光面、locality-aware 编排与流式内存、Gaia 两级缓存与查询合并、GLS/Q-W/psfsw、variance/ivar 链）是否属 RELEASE-01 发布阻断项——若"是"，当前实现不可发布；若"否"（属下一版本路线图），则 L2/L3 中依赖它们的条目不能按新文档判绿，须在发布说明中如实声明。
- **发布建议**：**暂不满足 `READY_FOR_OWNER_REVIEW`**（L2 不通过 + 视觉 FAIL + P0=16 OPEN）。待负责人对上述三项裁决、并明确哪些 P0 属本轮必修后，前台按裁决修复并复跑受影响层，再提交成品帧复检。
