# AstroCS 预发布验收报告（PROJECT-GOVERNANCE-02 / PRE-REL）

> 权威链：`ASTROCS_DESIGN.md` → `AGENTS.md` → `ENGINEERING_SPEC.md` → `CONTROL_PACK_SPEC.md` → `ACCEPTANCE_SPEC.md` → 本报告。
> 结论口径：**代理只可声明 `READY_FOR_OWNER_REVIEW`；发布决定属负责人**（ACCEPTANCE_SPEC §5.3 / AGENTS §5）。
> 证据落位：原始日志/产品在 `run/PROJECT-GOVERNANCE-02/PRE-REL/`（gitignore，不入库）；本报告与 `artifacts/acceptance/**` 摘要入库。

## 0. 结论摘要

| 层 | 目标 | 状态 | 依据 |
|---|---|---|---|
| L1 | 合成科学性 | 待机器门复核 | ctest 基线 442；本包修复的模块级红全部转绿 |
| L2 | 合成性能（G-RES-01） | **通过** | `run_monitored.py --gate-required --gate-workers 16` → verdict=pass，exit 0 |
| L3 | 小批量真实数据 E2E | **跑通；科学面有 1 项 P0 缺口** | 三命令 rc=0；Gaia 天测闭合 0.10–0.14″；权重链路受 PRE-F-01 阻塞 |
| L4 | 真实数据视觉 | **工具链就绪；全量运行未做** | `plane_stretch.py`/`plane_chunks.py` 落地并自证；全量 M42/GC 需负责人节点资源 |

**发布阻塞项（详见 `工程控制/PROJECT-GOVERNANCE-02/PRE_RELEASE_FINDINGS.md`）**：
`PRE-F-01`（P0，Phase1→Phase2 ivar 接线缺失）、`PRE-F-03`（P2，量纲/BUNIT 待裁）、`PRE-F-04`（P1，D08 标记载荷卡）。

---

## 1. 本包完成的工作

1. **文档集并入当前最高权威**：`AstroCS文档集 (1).zip` 的 11 个差异文件 + 新增 `ACCEPTANCE_SPEC.md` 逐字落地（`ASTROCS_DESIGN.md`/`AGENTS.md`/`ENGINEERING_SPEC.md`/`docs/ci/**`/`docs/plugins/**`），并同步根清单与 §7 根条目登记（`ci/root_manifest.json`）。
2. **W8 迁移收口**：`hips_properties* → lib/algorithms/coverage/`、`p3_resample.cpp` 编入 `astrocs_p3_rsmp`、链接序修正；构建 rc=0。
3. **三命令端到端跑通**（本包核心，此前从未跑通）：见 §4。
4. **L4 视觉工具链补齐**：`tools/quality/plane_stretch.py`、`tools/quality/plane_chunks.py`。
5. **机器门红项收敛**：见 §6（判据订正/锚点同步/陈旧测试修复，逐条留证）。

---

## 2. L1：合成数据科学性

- 构建：`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` → rc=0；`ninja -C build` → rc=0（W8 链接序修复后 4 个原本 link-fail 的目标转绿）。
- ctest 基线 **442**（TASK_LIST §0）。本包相关模块级红（`p1_nside_test`/`p3_sampler_cache_test`/`p3_output_test`/`p3_assembly_test`）随 W8 修复转绿。
- 复算命令与结果见 §6 机器门口径；`p1phot_performance` 为负载敏感项（独占下通过，负载下可能红——见 `OPEN_ITEMS F1`）。

## 3. L2：合成数据性能（G-RES-01）

- 命令：`python3 tools/monitoring/run_monitored.py --timeout 1800 --gate-required --gate-workers 16 --gate-selected-workers 16 --output run/PROJECT-GOVERNANCE-02/PRE-REL/l2_resource_gate.json -- ./build/astrocs normalize --json <M42/Red 帧> -y --events-jsonl`
- 结果：**verdict=pass, rc=0**；计算区间 208.8 s（>10 s 判定域）；`work_core_seconds=2718.3`。
  - p50 利用率 **0.9475**（≥0.90）；逐样本通过率 **0.7796**（≥0.70）；active threads 64（≥2）。
  - 记录项（`record_and_justify`，非硬失败）：均值利用率 **0.8138**（<0.85，契约明示该均值门未标定前不得硬失败）；连续 15.1 s <60% 窗但**无就绪线程积压**（不判 CPU 饥饿）。
- 证据：`run/PROJECT-GOVERNANCE-02/PRE-REL/l2_resource_gate.json`（含 `samples` 曲线、`host_probe`）。

## 4. L3：小批量真实数据端到端

**数据**：NGC1727 T2 Red 600 s 亮场 ×3 + T2 校准帧（bias/dark 600s/flat Red），构成完整数据块。

**可行性（三命令串行 rc=0）**：

| 命令 | rc | 墙钟 | 峰值 RSS | 证据 |
|---|---|---|---|---|
| `normalize` ×3 | 0 / 0 / 0 | 3:16 / 3:40（后两帧） | 2.6–3.2 GB | `l3_e2e3.log` |
| `mosaic` | 0 | ~1:17 | 0.74 GB | `l3_mosaic_export.log` |
| `export` | 0 | ~0:02 | — | `l3_mosaic_export.log` |

- 阶段间**只经磁盘产品 + manifest** 交接：Phase1 输出目录直接作为 Phase2 的 `hips_paths`，Phase2 输出直接作为 Phase3 的 `source.hips_dir`。
- 产物：Phase1 HiPS `signal/support` ×3（186 tiles 多阶）、Phase2 `signal/support`（2.3 G）、Phase3 平面 `output_phase3.fits`（1024×1024，CTYPE `RA---TAN`/`DEC--TAN`，CRVAL 73.091667/−69.589722，BUNIT ADU，2 HDU）。

**科学性**：
- **WCS 与已知天区对拍**（Gaia 锥匹配解算，非循环）：

  | 帧 | inliers/samples | RMS (″) | RMS (px) | max roundtrip (px) | CTYPE |
  |---|---|---|---|---|---|
  | 01 | 43/64 | 0.1408 | 0.1455 | 1.38e-10 | RA---TAN-SIP |
  | 02 | 47/64 | 0.1026 | 0.1061 | 1.29e-10 | RA---TAN-SIP |
  | 03 | 42/64 | 0.1198 | 0.1238 | 1.84e-10 | RA---TAN-SIP |
- **星点/PSF 抽检**：帧 01 检出 47081 源、PSF fast 拟合 4315 有效；median FWHM 3.52×3.70 px、median ellipticity 0.217（圆锐、无系统性拖尾）。
- **通量口径**：`photometry_applied=false, photscal=1.0`（中性，未施加相对流量归一）。
- **权重链路**：**未满足**——Phase1 CLI 不产出 variance/ivar 子产品（`p1_final.json: n_variance_tiles=0, n_ivar_tiles=0, uncertainty_available=false`），Phase2 `weight_mode=2` 显式拒绝；本报告的全链 rc=0 是经**显式登记**的 `legacy_allow_weight_fallback=true` 降级取得。见 `PRE-F-01`（P0）。

**性能**：全程资源记录归档（每阶段 `resource_summary.json`/`alloc_report.json`/`worker_balance.csv`）；真实负载下 G-RES-01 记录项无 enforce 级异常。

**并行确定性**：`tests/backend` 覆盖 1-worker vs N-worker 数值一致（`test_p2001_parallel_sampler.test_03_parallel_equals_reference` 等，210 tests OK）。

**原子性 / 失败路径**（rc≠0 + 零科学产物 + 逐字诊断）：

| 场景 | rc | 输出目录内容 | 诊断 |
|---|---|---|---|
| 缺 master flat | 3 | 仅 run 元数据（manifest `status=incomplete`、alloc/resource/run_context/worker_balance），**无科学产品** | `phase1 failed: node cal failed: cannot read master: /tmp/does_not_exist_flat.xisf` |
| 缺 mosaic 源 | 3 | 同上，无科学产品 | `phase3 failed: node properties failed: p3_sampler_open_ex: properties not found: …` |
| Phase2 缺 ivar（无降级） | 2 | 无完整产品 | `weight_mode=2 requires per-frame ivar products; 3/3 frames missing ivar` |

## 5. L4：真实数据视觉验收

- **工具链就绪**：`tools/quality/plane_stretch.py`（FITS→整幅非线拉伸 PNG + 缩略图，参数入 PNG 文本块/sidecar，可复现）、`tools/quality/plane_chunks.py`（整幅 PNG→固定网格分块 + 行列角标 + 缩略图 + `tiles_index.json`）。
  - 双向可假：缺输入 → rc=2；全 NaN → rc=2；grid=0 → rc=2；同输入重跑字节一致（`cmp` = YES）。
- **实测链路已跑通**（在 L3 真实产品上）：`output_phase3.fits → full.png(1024²) → 4×4 tiles + thumbnail`，证据 `run/PROJECT-GOVERNANCE-02/PRE-REL/L4_l3demo/`。
- **全量 M42 / Galaxy Center 未做**：资源与时间需负责人节点排期。
  - M42 共 196 亮场、GC 157 亮场；单帧 Phase1 产物约 **0.55–0.61 GB** ⇒ M42 全量 Phase1 约 **106 GB**、GC 约 **85 GB**；本节点 `/workspace` 可用约 108 GB，且单帧墙钟 ~3.5 min（196 帧 ≈ 11.4 h 仅 Phase1）。
  - 已备好 M42 T2 Red 六面板子集配置（`run/PROJECT-GOVERNANCE-02/PRE-REL/l4_configs/`）作为可复现入口。

## 6. 机器门

（见 §6 附：`ci/run.py --profile fast` 结果）

## 7. 未关项

见 `工程控制/PROJECT-GOVERNANCE-02/PRE_RELEASE_FINDINGS.md` 与 `OPEN_ITEMS.md`。
