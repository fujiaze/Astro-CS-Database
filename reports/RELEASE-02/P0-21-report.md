# P0-21 修复报告 — normalize 一组进一组出（N 帧 ⇒ N 个 HiPS 产品）

- 任务：RELEASE-02 / P0-21
- 依据：`ASTROCS_DESIGN.md` §3.3「数据块」+ §3.4「输出基数」（负责人明确：一组输入 light →
  一组 HiPS 输出，每一帧输入对应一个 HiPS 产品）+ §3.4「可串行衔接」；
  `ENGINEERING_SPEC.md` §8 fail-closed、§9 不留半成品。
- 工作目录：`/workspace/Astro CS Database`
- 状态：代码已改；定点验证已跑（见 §5）；**按前台要求，最终全量构建/ctest 由前台统一执行**。

---

## 1. 根因

`lib/infrastructure/scheduler/src/module_adapters.cpp` 中 Phase1 的**决定性操作器只取
`input_lights[0]`**，而输入合同（`input_lights`）是**帧数组**：

| 操作器 | 修复前 | 修复后 |
|---|---|---|
| `p1_op_calibrate` | 标定循环处理全部帧，但**母版单位域证据只用首帧探测**（`lights.front()`） | 逐帧探测（`mu_light_probes`）并逐帧过 `check_master_domain` |
| `p1_op_wcs` | **只解/只写首帧** `p1_wcs.json` | 逐帧解算 + 逐帧落 `<frame_dir>/p1_wcs.json` |
| `p1_op_drizzle` | **只 drizzle 首帧**，只写 1 个根级 HiPS | 逐帧 drizzle + 逐帧落 `<frame_dir>/` 标准 HiPS |
| `p1_op_writer` | 只校验根级 `signal/properties`、只写 1 个 `p1_final.json` | 逐帧校验 + 逐帧 `p1_final.json` + 聚合 `p1_products.json` |

后果（L4 实测口径）：12 个配置共 49 帧 `input_lights`，每配置只 drizzle 1 帧、只写 1 个
HiPS ⇒ 49 帧只产 12 个产品，**静默丢弃 37 帧且无 WARN/ERROR**（违反 fail-closed）。

---

## 2. 输出落位结构（前台生成 L4 输入 / mosaic 配置请用此口径）

```text
output_dir/
├── calibrated_<base>.fits            # 逐帧中间产物（cal 节点；按帧命名，不互相覆盖）
├── cleaned_<base>.fits               # 逐帧中间产物（cos 节点）
├── p1_sources.json / p1_psf.json     # 聚合目录（含全部帧，按 file 字段逐帧）
├── p1_flux.json / p1_phot.json
├── p1_snr.json
├── <frame_key>/                      # ★ 每帧一个 HiPS 产品目录（本次新增）
│   ├── p1_wcs.json                   # 该帧 WCS（wcs 节点产物）
│   ├── p1_stack.json                 # 该帧 drizzle provenance（nside/precision/SIP…）
│   ├── signal/ support/ metadata.fits Moc.fits properties   # 标准 IVOA HiPS 树
│   └── p1_final.json                 # 该帧产品事实面（n_tiles/products/frame_id…）
└── p1_products.json                  # ★ 聚合结构化 JSON（列出全部 N 个产品路径）
```

### `<frame_key>` 命名规则
- 取输入 light 的**基名去掉扩展名**（`light_1.fits` → `light_1`）；
- 字符白名单化：仅保留 `[A-Za-z0-9_.-]`，其余字符 → `_`；空/./.. → `frame`；
- **同块内 `frame_key` 必须唯一**，否则 `DATA` fail-closed（禁止两帧共用目录互相覆盖）。

### `p1_products.json` 字段（schema `DATA-P1-PRODUCTS`）
```jsonc
{
  "schema": "DATA-P1-PRODUCTS",
  "entry": "hp_drizzle_run_phase1_hips",
  "output_dir": "<out>",
  "n_frames": 3,            // == input_lights 帧数
  "n_products": 3,          // == n_frames（可核对）
  "count_consistent": true,
  "hips_paths": [           // ★ 直接作为 mosaic 配置的 "hips_paths" 消费
    "<out>/light_1", "<out>/light_2", "<out>/light_3"
  ],
  "frames": [
    {"frame_id": "light_1", "input_light": "<abs path>", "hips_path": "<out>/light_1",
     "properties": "<out>/light_1/signal/properties", "final": "<out>/light_1/p1_final.json",
     "nside": 512, "n_tiles": N, "n_support_tiles": N,
     "n_variance_tiles": 0, "n_ivar_tiles": 0, "uncertainty_available": false}
  ],
  "filter_passband": "..."
}
```

**衔接用法**：`mosaic` 配置的 `hips_paths` = `p1_products.json["hips_paths"]`（逐帧 HiPS
产品目录数组），无需再做任何路径推导。

---

## 3. 改动面（file:line）

### 3.1 `lib/infrastructure/scheduler/src/module_adapters.cpp`
- **L105 / L120**：新增 `<cctype>` / `<set>` 头。
- **L1200–1240**：新增 P0-21 辅助函数
  - `p1_frame_key(light)`（L1204）— frame_key 派生 + 字符白名单；
  - `p1_frame_dir(doc, light)`（L1221）— `output_dir/<frame_key>`；
  - `p1_require_unique_frame_keys(doc)`（L1227）— 重复 frame_key fail-closed。
- **L1361–1371**：`p1_require_lights` 追加 frame_key 唯一性门（覆盖 cal/cos/psf/snr/drz）。
- **L1577 `p1_op_calibrate`**：
  - L1677–1720：母版单位域证据由「首帧探测」改为**逐帧探测**，逐帧过 `check_master_domain`
    （bias/dark），失败信息带具体帧路径；
  - L1752–1753：manifest 增 `light_frames_probed`（= 帧数，可核对）。
- **L2485 `p1_op_wcs`**：
  - L2490–2493：frame_key 唯一性门；
  - L2624–2650：显式 WCS 路径逐帧校验可读性并逐帧写 `<frame_dir>/p1_wcs.json`；
  - L2706–2945：真实 ipv 路径改为**逐帧循环**（逐帧读入 → 逐帧指向 → 逐帧求解 →
    逐帧 roundtrip/前向交叉门 → 逐帧落盘），任一帧失败整体 fail-closed。
- **L3326 `p1_op_drizzle`**：
  - L3330–3333：删除根级 `p1_wcs.json` 预读（改为循环内逐帧读）；
  - L3470–3758：**逐帧 drizzle 循环**——每帧读 `<frame_dir>/p1_wcs.json`（回退
    `config.wcs`）→ 逐帧 `hp_drizzle_run_phase1_hips` 直写 `<frame_dir>/` →
    逐帧 `p1_stack.json`；manifest 增 `n_frames`/`stack_artifacts`/`frames`。
- **L3765 `p1_op_writer`**：
  - L3768–3776：要求非空 `input_lights` + frame_key 唯一性门；
  - L3778–3930：逐帧校验 `<frame_dir>/signal/properties`、逐帧写 `p1_final.json`；
  - L3931–3950：写聚合 `p1_products.json`（`DATA-P1-PRODUCTS`），并断言
    `n_products == input_lights.size()`（不一致即 `DATA` fail-closed）。

> 未改动：`p1_op_cosmetic` / `p1_op_star_psf_impl` / `p1_op_photometry` / `p1_op_noise`
> 本就逐帧循环（聚合 JSON 按帧登记）；逐帧中间产物命名 `calibrated_<base>` /
> `cleaned_<base>` 保持不变（已按帧区分）。

### 3.2 测试
- `tests/unit/p1001_real_nodes_test.cpp`
  - 新增 `frame_root(fx, base="light_1")` 辅助（每帧产品根）；
  - 既有断言按新落位更新（`output_dir/signal` → `output_dir/<frame_key>/signal`，
    同理 `p1_stack.json`/`p1_final.json`/`p1_wcs.json`）；
  - 新增 `test_p0_21_multi_frame_one_hips_per_input()`：
    - **正例**：3 帧不同幅度星场 → `n_frames==3 && n_products==3`、
      `hips_paths.size()==3`、3 个 `signal/properties` 存在、3 个产品 signal
      平面**两两不同**（不是同一帧写三次）；
    - **负例 1**：第三帧路径不存在 → 全链失败、不写 `p1_products.json`；
    - **负例 2**：drizzle 直接消费含不存在帧的输入 → 必须失败（不得静默跳过）。
  - 全链用例（2 帧）增补两帧产品与 `p1_products.json` 断言。
- `tests/cli/test_phase1_inprocess.py`：test_07/test_08/test_10 改为逐帧产品路径 +
  增 `p1_products.json` 计数断言。
- `tests/cli/test_phase123_pipeline.py`：normalize 产品路径改为 `<out>/light_N`，
  mosaic 改消费逐帧产品目录（test_01 增计数断言）。
- `tests/unit/p2001_real_nodes_test.cpp`：`make_p1_fixture` 中 P1 真实节点产出的
  HiPS 根由 `output_dir` 改为 `output_dir/light_N`（新落位）。

---

## 4. 关于 `call_count==1` 断言（前台 point 5）

**结论：保持不变，且不应随帧数变。** 依据：`call_count` 是**调度器对模块（节点）的
调用次数**（RT trace 语义），不是帧数。IR 有 7 个节点，每节点被调度一次；帧循环是
**节点内部**行为（`for (const auto& l : doc["input_lights"])`）。若把 `call_count` 改成
随帧数变，等于断言"调度器把同一节点调用 N 次"，这与 ARCH-P0-001 冻结的节点化 IR
语义相反，属**错误断言**，故不放宽、不改动。

帧基数关系改由**产品面**断言表达（这是正确行为）：
- `p1_products.json.n_products == input_lights.size()`；
- 逐帧 `signal/properties` 存在；
- 3 个产品 signal 内容两两不同。

---

## 5. 验证结果（定点；全量由前台统一执行）

### 5.1 全量 ctest（修复中途跑过一次，供参考）
- 命令：`TMPDIR=/dev/shm/astrocs_tmp ctest --test-dir build --output-on-failure -j 2`
- 结果：**99% tests passed, 1 tests failed out of 460**（459 通过）。
  唯一失败 `#53 p2001_real_nodes`，原因 = `make_p1_fixture` 仍把 `output_dir` 根当作
  P1 HiPS 根（新落位下应为 `output_dir/light_N`）；已按新落位修正，并**单独复跑通过**
  （`P2-001 REAL NODES PASS`）。因此预期最终全量为 460/460。
- 日志：`run/RELEASE-02/P0-21/logs/ctest_full.log`。

### 5.2 定点复跑
| 用例 | 结果 | 日志 |
|---|---|---|
| `p1001_real_nodes`（含新 P0-21 正/负例） | PASS（0 CHECK failed） | `logs/p1001_after5.log` |
| `p2001_real_nodes`（修正后） | PASS | `logs/p2001_after.log` |
| `pytest test_phase1_inprocess.py test_phase123_pipeline.py` | 19 passed | `logs/pytest_cli2.log` |
| **故障注入**（drizzle 恢复为只取首帧） | **RED**：writer 报 `light_2/signal/properties` 缺失、`p1_products.json` 不存在、计数断言全红 | `logs/p1001_faultinject.log` |

### 5.3 「N 帧进 N 产品出」CLI 实测（真二进制 `build/astrocs`）
脚本：`run/RELEASE-02/P0-21/evidence_n_frames.py`（日志 `logs/evidence_n_frames3.log`）：
```text
positive rc = 0
p1_products.schema = DATA-P1-PRODUCTS
p1_products.n_frames = 3
p1_products.n_products = 3
p1_products.count_consistent = True
  hips_path: <out>/light_1
  hips_path: <out>/light_2
  hips_path: <out>/light_3
signal/properties count on disk = 3
negative rc = 3
negative stderr: astrocs: phase1 failed: node cal failed: cannot read light: <...>/does_not_exist.fits
```
证据落位：`run/RELEASE-02/P0-21/evidence/`（`p1_products.json`、`products_tree.txt`、
`negative_stderr.txt`）。

内容互不相同（3 个不同星场 → 3 个不同 signal 平面）由单元测试
`test_p0_21_multi_frame_one_hips_per_input` 锁定（通过）；CLI 证据脚本只证基数关系。

---

## 6. 未构建验证说明（按前台新分工）

- 收到前台指示后，**已停止**新的 `ninja` / `ctest` 全量；本报告中的全量数字来自指示前
  已跑的那一次（459/460 + 已单独修正并复跑通过的 p2001）。
- 报告与代码均已落地，工作树保留；**最终 `ninja -C build` + `ctest` 全量请前台统一执行**。
- 若前台在合并其他工作包后需要，我可提供最小定点复跑命令（见 §5.2）。
