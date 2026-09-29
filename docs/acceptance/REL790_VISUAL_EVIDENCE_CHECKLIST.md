# REL-790 成品帧视觉验收 · 证据包清单

> 上游：`ACCEPTANCE_SPEC.md` §6.1（产品生成链）/ §6.2（视觉验收清单六项）/ §6.3（验收与发布决策）、
> `run/FINAL-07/pkg/tasks/REL-790_目视验收收口.md`（控制包任务）
> 机器实现：`eng/tools/acceptance/rel790_pack_check.py`（RP-01..RP-08）
> 清单（机器可读）：`eng/tools/acceptance/rel790_checklist.json`（帧清单 81 帧 + 必记字段）
> 纪律：本清单**只规定必须看什么、必须记什么**，不改任何冻结阈值，不代替负责人目检。
> 负责人已定：**全量端到端未跑完之前不重跑、不判档位**；本清单不触发任何运行。

## 0 两类记录

| 类 | 键 | 数量 | 谁产生 |
|---|---|---|---|
| A 逐输入帧判定记录 | `frame_records` | 81（M42 49 + 银心 32） | 端到端运行 + 档位判词记录 |
| B 目检图块记录 | `visual_panels` | ≥2（每个天区至少整幅 1 张） | agent 初审 / 验收方目检 |

## 1 A 类：必须逐帧记录的帧（81 帧）

| 天区 | 根目录 | 计划帧（分母） | 检验点 |
|---|---|---|---|
| M42 T2+T3 Red | `testdata/M42_T2T3_mosaic_Flying_dutchman` | **49**（T2 16 + T3 33） | 亮星云、高动态范围、密集星场 |
| Galaxy Center T4 Red | `testdata/Galaxy_Center_T4` | **32** | 极密集星场、背景结构、大尺度马赛克 |

逐帧名单（机器可读、全量列在 `rel790_checklist.json` 的 `frames[]`，此处只给前缀）：

~~~text
M42: testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1..M6/M42_M{1..6}_T2_flying_dutchman-<date>@<hhmm>-300S-Red.fts   (16)
     testdata/M42_T2T3_mosaic_Flying_dutchman/T3/...-300S-Red.fts                                                      (33)
GC : testdata/Galaxy_Center_T4/lights/panel{1..n}/Galaxy_Center_mosaic{n}_T4_flying_dutchman-<date>@<hhmm>-180S-Red.fts     (32)
~~~

**分母纪律**：分母 = 计划帧 = 实际入册 Red 帧数（49 / 32）。用「抽样帧」或「跑通帧」作分母即判红。
少一帧的记录 ⇒ RP-02 判红。

## 2 A 类：每帧必记字段

| 字段 | 类型 | 为什么必记 |
|---|---|---|
| `frame_id` | string | 逐帧可对账 |
| `verdict` | `ACCEPT` / `REJECT` / `NOT_EVALUATED` | 帧级判词（三值，不许只写通过/失败） |
| `n_pairs` | int / null | WCS 内点数（冻结门 `n_pairs<12` 的读数侧） |
| `rms_px` | number / null | WCS 拟合残差（冻结门 `rms_px>0.5` 的读数侧） |
| `rejection` | object / null | `verdict=REJECT` 时必填（见 §3） |
| `evidence` | array | 逐帧日志/侧车/单帧运行记录；`run/` 下必须带 `sha256` |

## 3 拒绝帧必记（这是本清单的重点）

| 字段 | 取值 | 纪律 |
|---|---|---|
| `rejection.gate_id` | string | **必须能在 `frozen_gate_inventory.json` 里查到**（RP-04） |
| `rejection.reason` | string | **逐字引产品日志的拒绝行**；转述版判红 |
| `rejection.classification` | `product_behavior` / `defect` | **`unknown` 不接受**（RP-04 判红） |
| `rejection.exit` | `stop_work` / `registered` | 停工还是走登记流程 |
| `rejection.deterministic_replay` | bool | 重跑是否逐位复现（区分偶发 vs 确定） |

「拒绝是产品行为还是缺陷」的判别口径：

- **产品行为** = 门按设计工作，该帧确实不满足冻结定义（如某帧确实落在 `matched<30` 硬回退且残差超门），产品按 fail-closed 拒；
- **缺陷** = 门本身或喂给门的输入有问题（如五帧越闸定性报告的结论：非点源成员进入选星向量，把 rms 从 0.106 抬到 0.6554）。
判据落不到这两类之一 ⇒ `unknown` ⇒ **判红**，因为它没有可执行的下一步。

## 4 B 类：必须由人看的图块与每块必记字段

### 4.1 必须看的图块

| 图块 | 数量 | 说明 |
|---|---|---|
| 整幅 PNG | 每天区 ≥ 1（M42 + 银心 ⇒ ≥ 2） | 拉伸脚本输出，固定参数可复现 |
| 切块 PNG | 按固定网格全覆盖（行列编号 + 整幅缩略图） | 两组天区用同一套脚本 |
| 疑问区域裁剪放大图 | 有疑问就有 | 低分辨率整幅图不足以支撑结论（`ACCEPTANCE_SPEC` §6.1 末条） |

### 4.2 每块必记字段

| 字段 | 说明 |
|---|---|
| `panel_id` / `kind`（`full` / `tile`） | 图块编号与类型 |
| `image` | `{path, sha256}` —— PNG 路径 + 内容指纹 |
| `inspector` | 谁看的（agent 初审 / 验收方） |
| `items` | **六项逐项判词**（见 §4.3），缺一项判红 |
| `zoom_note` | 有疑问区域是否裁剪放大后再读 |
| `reject_reason` | 任一项不合格时的原因（须分类 product_behavior / defect） |

### 4.3 六项目检项（正本 = `ACCEPTANCE_SPEC.md` §6.2，本清单不复制阈值）

`no_black_hole / no_bright_spot / no_seam / star_quality / background_geometry / global_impression`

**无接缝项需同时附机器门逐边度量 JSON**（`rel_step` + `exclude` + `margin_px`），只有目检即判红：
即 `CHK-L4-SEAM-FOOTPRINT` 的产物面（RP-07）。该门当前在零产品时会整步跳过 ⇒ 附件缺失就是没有机器证据。

## 5 包级必交（7 项）

`pack_id / dataset / binary_and_profile / seam_machine_metrics / coverage_metrics / stage_logs / tier_verdict_record`

- `binary_and_profile` 必须同时给 `binary` + `head_sha` + `cpu_profile_commit`（RP-08）：三者缺一则「同二进制性」不可复核（画像过期 rc=5 的处置面）；
- `coverage_metrics` = `covered_px / total_px / finite_fraction`（覆盖度轴分母）；
- `tier_verdict_record` = 本批对应的档位判词记录（由 `tier_verdict_gate.py` 判绿）。

## 6 可执行证据

~~~bash
python3 eng/tools/acceptance/rel790_pack_check.py --self-test   # 正例 1/1 + 注入负例 8/8
python3 eng/tools/acceptance/rel790_pack_check.py --pack <pack.json>
~~~

实测：`RP_SELFTEST_PASS: 正例=1/1 注入负例=8/8`（负例含：静默缩分母、拒绝无原因、拒绝分类 unknown、目检漏项、图块文件不存在、无接缝无机器度量、缺画像指纹、缺 rms_px）。

## 7 未做与原因

- **未跑全量端到端**、未生成任何新 PNG：负责人明令「全量端到端未跑完之前不重跑」；本任务只出清单与判据。
- **未注册进 CI**（`eng/ci/checks.json` 本轮禁改）⇒ 由前台登记后再进 CI。