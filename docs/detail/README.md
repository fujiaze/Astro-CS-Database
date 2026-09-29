# 详细设计集（docs/detail）

> 上游：`docs/science/`（科学主张）与 `docs/engineering/`（工程口径）。
> **本集任何与上层冲突的表述都不成立**——细节向下对齐，不向上另立权威。

## 1 这一集是什么，不是什么

**是**：二级详细设计集。由科学集与工程集两套推理出来，自解释、含函数名与算子级实施规范，
直接指导代码——模块说明卡、插件工作细节、阶段间接口合同、三阶段详细设计、验收细则、运维与诊断。

**不是**：

- 不是权威来源。口径冲突时以 `docs/science/` 与 `docs/engineering/` 为准。
- 不是模块代码的替代。实现代码在 `lib/`，本集解释它为什么长这样。
- 不是实验记录与数值产出。那些在 `实验/`。

**硬边界一句话**：本集回答「具体怎么做」，不回答「凭什么这么做」——后者是上层两集的职责。

## 2 篇目清单

按子域组织。全集共 90 篇 = 88 篇迁入件 + 本 README 与 `00_INDEX.md` 两份招牌件。
各插件目录的「集 README」是有意合并的产物：迁移 A 段把原 26 份目录级招牌件消化为
3 份集 README 与 1 份 `merged_TROUBLESHOOTING.md`，故每目录文件数 = 声明篇数 + 1。

| 子域 | 目录 | 解决什么问题 |
|---|---|---|
| 三阶段详细设计 | `PHASE1_DETAILED_DESIGN.md`、`PHASE2_DETAILED_DESIGN.md`、`PHASE3_DETAILED_DESIGN.md`、`PRODUCT_STORAGE_FORM.md` | 每一阶段的目标态怎么落地、阶段边界在哪 |
| 阶段专用详细设计 | `UNIFIED_MODEL.md`、`STAR_DETECTION_IMPL_DESIGN.md`、`LOG_AND_ERROR_SYSTEM.md` | 统一配置模型、星检测实现、日志与错误系统怎么设计 |
| 模块说明卡（按 module_id） | `registry/` | 生产实现登记：每个算法模块解决什么问题、落位在哪、依赖什么 |
| 插件工作细节 | `algorithms_phase1/`、`algorithms_phase2/`、`algorithms_phase3/`、`infrastructure/` | 每个阶段的具体步骤、算子次序、参数与失败处置 |
| 文档—代码锚设施 | `anchors/` | 行锚合同与机读登记（另 3 件机读件与检查器未迁入，见 §5 待裁决 6） |
| 插件域索引 | `00_INDEX.md` | 四个插件目录 23 篇的模块总表（**只覆盖插件域**；集入口是本 README） |

根级另有两类卡片，**不在任何子目录内**，故在此逐条列出（此前被「都在独立目录里」一句免列，导致 23 篇不可达）：

**模块说明卡（`# Module: <名>` 体例，职责 / 非职责 / Public API / Data contract），15 篇**：

| 篇 | 解决什么问题 | 篇 | 解决什么问题 |
|---|---|---|---|
| `acr.md` | 异构计算运行时：kernel registry、CPU/GPU 调度、device executor | `orchestrator.md` | Phase1 编排：stage1.json 驱动各节点 |
| `astro_image_io.md` | 唯一 I/O 层：FITS/XISF/ahpx 读写、压缩、HiPS 读/写 | `phase1_session.md` | Phase1 装配：io_read→calibrate→cosmetic→io_write |
| `calibration.md` | masterBias/Dark/Flat 生成与单帧图像校准 | `phase2.md` | Phase2 多帧统一模型编排 |
| `common.md` | 共享基础库：HEALPix 核心、SHA-256、标量精度抽象 | `photometric_calib.md` | 测光定标：星表配对与稳健零点求解 |
| `dynamic_psf.md` | Moffat4 动态 PSF 建模与拟合质量代理 | `plate_solve.md` | 星表匹配 + TAN/SIP plate solve → WCS |
| `gaia_xpsd_client.md` | 本地星表解析、缓存、坐标/历元语义（离线零网络） | `snr_estimator.md` | 三层噪声模型与质量结构体落位 |
| `healpix_browser_qt.md` | HiPS/HEALPix 球面浏览器（Qt6 + OpenGL） | `star_detector.md` | Phase1 单帧 light 全图检测 |
| `healpix_drizzle.md` | 球面 Drizzle：HEALPix NESTED 重投影与通量守恒累加 | | |

**模块合同页（`# 模块 astrocs.pX.Y` 体例，身份与合同落位），8 篇**：

| 篇 | 对应 module_id 合同值 | 篇 | 对应 module_id 合同值 |
|---|---|---|---|
| `phase2_samp.md` | `astrocs.p2.sampling` | `phase3_proj.md` | `astrocs.p3.projection` |
| `phase2_upm.md` | `astrocs.p2.upm` | `phase3_rsmp.md` | `astrocs.p3.resample` |
| `phase2_rej.md` | `astrocs.p2.rejection` | `phase3_fits.md` | `astrocs.p3.fits_writer` |
| `phase2_int.md` | `astrocs.p2.integration` | `hips_p2.md` | `astrocs.p2.hips_writer` |

另有：

- `HIPS_BROWSER.md` —— HiPS 浏览器组件的说明（与 `docs/engineering/observability/` 不同面，
  一个讲组件行为，一个讲可观测采集）。
- 合并后的排障手册 `merged_TROUBLESHOOTING.md` —— 由原两篇同名文件合并，
  一篇是「症状 → 定位 → 修复」速查，一篇是「高风险故障场景覆盖门 + 通用定位顺序」。见 §5 待裁决。

## 3 从哪看起

**要改某个算法模块**：从 `registry/` 找到该模块的说明卡，卡里给出边界与依赖；
再进对应阶段的 `algorithms_phase*/` 看具体步骤与失败处置。

**要接一条阶段间接口**：先读 `PRODUCT_STORAGE_FORM.md` 定产品形态，
再到上层 `docs/engineering/data/`、`io/` 读合同正本——本集只解释该合同在本阶段怎么落地。

**要弄清某个配置键从哪来**：读 `UNIFIED_MODEL.md`，它给出三阶段共用的配置模型。

**排查线上问题**：读 `merged_TROUBLESHOOTING.md` 的症状速查表；
若表中没有该症状，去上层 `docs/engineering/RELEASE_STATUS.md` 与
`docs/engineering/VERSIONING.md` 确认版本与已知限制。

**要写一个新的模块卡**：照 `registry/` 里已有卡的格式写，并满足上层追溯层定义
（见 `docs/engineering/TRACEABILITY_SPEC.md`）。

## 4 可信度现状

**已审定**：与上层一致、且有对应测试的篇目。模块说明卡与阶段插件细节大多属此列，
因为它们逐条对着实现与测试写。

**待审或待裁决**：见 §5。此外，本集在迁移后尚未重跑「对上层覆盖度」核查，
暂不声明「上层每条要求都有对应细节篇」这一覆盖性结论。

**会清除的脚手架**：`algorithms_phase*/` 与 `infrastructure/` 下的编号前缀（`01`..`23`）
是历史排序，是否仍有语义需裁决（见 §5）；若裁决为纯历史，编号将被去掉。

## 5 需要负责人裁决的点

1. **插件目录的编号前缀**：`algorithms_phase1/`、`algorithms_phase2/`、`algorithms_phase3/`、
   `infrastructure/` 下的文件名带 `01`..`23` 编号。需裁决编号是历史排序还是仍有语义；
   若无语义，去掉编号会改变大量交叉引用。
2. **模块卡是机读生成件还是人工维护**：`registry/` 下 27 张卡若是机读生成，
   它们算不算「文档」需裁决——这决定它们是否受文档治理与索引门约束。
3. **锚设施的归属**：`anchors/` 服务的是全部文档而非某个模块，物理上更像工程集治理设施。
   现行落位表判 detail，迁移前置表判 engineering。需裁决。
4. **两篇 `TROUBLESHOOTING.md` 的合并形态**：合并为单篇（当前做法），
   还是拆回两篇——「症状速查」与「高风险故障覆盖门」是两种阅读用途，合在一篇会互相稀释。
5. **根级两篇排障手册的现状缺陷（迁移时已查明，合并稿已修正）**：
   合并前其中一篇的常用命令段有 4 条死路径，3 条指向早已不存在的旧目录结构，
   另 1 条是路径漂移（文件存在，目录已迁移）；已用文件系统与版本库索引两种读法核实。
   `docs/detail/merged_TROUBLESHOOTING.md` 已把它们改写为 `ctest --test-dir build -R` 调用。
6. **`anchors/` 迁移只完成一半**：集 README 声明 4 件，实际迁入 2 件。
   `anchor_contract.json`、`unresolved_registry.json`、`check_doc_line_anchors.py`
   仍留在 `docs/algorithms/anchors/`。需裁决是补迁到 `docs/detail/anchors/`，
   还是正式承认锚机读件与检查器归工程集治理面（与本条第 3 项的归属裁决合并处理）。
   现状：`anchors/README.md` 已按实际落点改写，**未擅自搬动文件**。
7. **根级两类卡片（15 篇模块说明卡 + 8 篇模块合同页）散在集根目录、不在任何子目录内**：
   迁移前它们应是各模块目录的说明页，迁移后留在 `docs/detail/` 根级。需裁决是就地保留
   （本 README §2 已逐条列出），还是收进 `registry/` 或新设 `modules/` 子目录。
   在裁决前本 README 按「实际落点」如实列出，不替负责人决定归属。

## 6 下钻指引

- 科学公式、常数、容差与判据：`docs/science/`。本集出现数值时，权威在那边。
- 行为合同、接口字段、门禁定义：`docs/engineering/`。
- 索引与分层规则：`ENGINEERING_SPEC.md` §8（仓库根，链外）与
  `docs/engineering/DOCUMENT_GOVERNANCE.md`。
- 实现代码：`lib/`。本集说「为什么这样写」，`lib/` 说「实际写了什么」，
  两者不一致时以 `lib/` 为事实、以本集为待订正对象。
- 追溯层定义（一条主张怎么追到代码与测试）：`docs/engineering/TRACEABILITY_SPEC.md`。

---

> 本 README 只写现行设计，不含裁决记录、订正流水、任务编号与日期（`DOCUMENT_GOVERNANCE.md` §5）。
