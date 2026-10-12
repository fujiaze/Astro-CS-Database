# DEV-MODE-01 — 运行前几何校验（E1/E2）与校准取数静默回退（E3）实测证据

本目录是本单（工程线·运行前几何校验 + 发布期去掉静默回退）的实测证据包。
全部读数由 `/tmp/e1check/` 的同一批配置与运行产生；大二进制已删，只留配置、日志与 run manifest。

## 文件用途

| 文件 | 用途 |
|---|---|
| `cfg_bad.json` | **反例 E1a**：1 帧 light；`master_flat` 换成 4096×4112（其余母版 4096×4096）⇒ 母版之间不一致 |
| `cfg_lightbad.json` | **反例 E1b**：母版全一致（4096×4096）；唯一的 light 换成 4096×4112 ⇒ light vs master 不一致 |
| `cfg_ok.json` | **正例对照**：1 帧 light + 全部母版 4096×4096（尺寸一致） |
| `bad.log` | `cfg_bad.json` 的完整 stdout+stderr |
| `lightbad.log` | `cfg_lightbad.json` 的完整 stdout+stderr |
| `ok.log` | `cfg_ok.json` 的完整 stdout+stderr |
| `out_bad__acsd_run_*.json` | 反例 E1a 的 run manifest（含完整错误串） |
| `out_light__acsd_run_*.json` | 反例 E1b 的 run manifest（含完整错误串） |
| `out_ok__acsd_run_*.json` | 正例的 run manifest |
| `samples_geometry_check.py` | 优先 3 的复核脚本：用 `cd`+纯 TAN 与 `cd`+TAN+SIP 两种前向复现 `p1_wcs.json#samples` 的 `ra/dec` |

三份配置都由 `run/PHOT-FIX/p1_rerun_config.json` 的 `blocks[0]` 截成 **1 帧**派生，
未手工挑选数据目录；反例 master 由 astropy 写成（`NAXIS1=4096, NAXIS2=4112`）。

## 三组实测读数

| 组 | 命令 | RC | `io_read` 命中 | 逐帧产物数 | 是否开工前失败 |
|---|---|---|---|---|---|
| 反例 E1a | `./build/linux-control/acsd normalize -y --json cfg_bad.json` | **3** | **0** | **0** | 是 |
| 反例 E1b | `./build/linux-control/acsd normalize -y --json cfg_lightbad.json` | **3** | **0** | **0** | 是 |
| 正例对照 | `./build/linux-control/acsd normalize -y --json cfg_ok.json` | **0** | 23 | 正常产出 | 不误报 |

反例的「逐帧产物数 = 0」证据：运行目录里只剩 run 级簿记（`acsd_run_*.json`、`resource_summary.json`、
`alloc_report.json`、`alloc_samples.csv`、`resource_timeseries.csv`、`worker_balance.csv`、`run_context.json`），
**没有任何帧产品**；日志里 `io_read` 阶段命中数为 0 ⇒ 校验在逐帧处理之前就失败。

### 反例错误串（点名文件 + 双方尺寸）

E1a（母版之间），取自 run manifest 的 `error.message`：

```
phase1 failed: node cal failed: prerun: master frame size mismatch:
master 'master_bias' (/workspace/Astro CS Database/testdata/T2 calibration files/masterBias_BIN-1_4096x4096.xisf) is 4096x4096
but master 'master_flat' (/tmp/e1check/masterFlat_bad_4096x4112.fits) is 4096x4112
```

E1b（light vs master）：

```
phase1 failed: node cal failed: prerun: light vs master size mismatch:
light '/tmp/e1check/masterFlat_bad_4096x4112.fits' is 4096x4112
but master 'master_bias' (/workspace/Astro CS Database/testdata/T2 calibration files/masterBias_BIN-1_4096x4096.xisf) is 4096x4096
```

## 优先 3：`p1_wcs.json#samples` 几何复核（改前/改后回归判据）

复核对象 = 修后二进制跑 `cfg_ok2.json`（`cfg_ok.json` 的第 2 次运行，输出到 `out_ok2`）产出的
`p1_wcs.json`，`wcs_source='ipv'`、`sip.order=3`、`ctype=RA---TAN-SIP`、`n_samples=64`。

| 判据（`max|samples − 复现前向|`，场角） | 改前（ipv 路径漏 SIP） | **改后（实测）** |
|---|---|---|
| `cd` + **纯 TAN** 复现 | **0.0000″**（samples 本就不含 SIP，故完全自洽） | **24.9086″** |
| `cd` + **TAN+SIP** 复现 | ~16″ 量级（SIP 在场角的作用量） | **0.003074″** |

两个数**互换了角色**：这就是修复的判别特征。改后产物自报
`max_sip_effect_deg = 0.006919 deg = 24.91″`，与「纯 TAN 复现误差 24.9086″」一致，
说明 SIP 已真实进入 samples；而 `max_forward_cross_deg = 2.8e-14`（含 SIP 产品前向 vs 独立含 SIP 参考解）。

「改前」两个数取自 lead 的独立复核（用 `cd`+TAN 复现 samples 误差 0.0000″；SIP 在场角约 17 px ≈ 16″），
本次**未重建改前二进制**复测 —— 未复测即如实标注。

## 本单代码改动要点（`lib/**`）

- `p1_product_forward_sip()` 新增为**唯一**的「含 SIP 完整前向」实现；
  explicit_config 与 ipv 两条路径都调它（AGENTS §13「同一件事只允许一份实现」）。
- ipv 路径：SIP 系数构造**上移到 samples 循环之前**；samples 的 `ra/dec` 改为含 SIP 的完整前向；
  交叉门的参考解改用含 SIP 版本；roundtrip 断言按「无 SIP 才有 1e-6 px 契约」加守卫。
- **判别性断言（非恒真）**：`sip.present && sip.order>0` 时，若非 SIP 与含 SIP 两条前向
  的最大角距 ≤ 1e-9 deg，即 fail（"SIP coefficients present but inoperative"）。
  两条路径都加了。理由见代码注释与
  `docs/detail/registry/acsd.phase1.wcs-platesolve.md`《`p1_wcs.json` 的 `samples[]` 几何契约》。
- 标签漂移修复：`forward_cross_ref` 由 `p1_tan_forward_reference` 改为实调名
  `p1_tan_forward_reference_sip`；新增 `max_sip_effect_deg` 与 `sample_geometry` 两个读数键。

编译：`flock /tmp/astrocs_build.lock cmake --build build/linux-control --target acsd -j 8` → RC=0。

## 追加登记 · 曲线键迁移后复跑（R4 预备）

本单元三份配置原先使用**已退役**的曲线文件路径键 `filters_json`
（指向已退役聚合文件 `eng/packaging/config/filters.json`），且未声明 QE ⇒ 按现状**不可复跑**
（测光节点会以 `PHOT_CONFIG_QE_MISSING` 中止）。本单元的用途是**可复跑的判据**（E1a/E1b 反例 +
正例对照，判据 = 开工前具名失败 / 正例不误报，README 已给出复现命令），故按当前口径**改对配置**：

- 三份配置的 `photometry.fit`：删 `filters_json`，加 `qe_name`，`filter` 不变 ⇒
  `{"enabled": true, "gaia_data_dir": …, "filter": "Baader R", "qe_name": "KAF-16803"}`
  （QE 键依据 T2 相机 = FLI Proline 16803；曲线库 = 运行期扫描
  `<可执行文件目录>/config/response_curves/`）。
- **归档日志与 run manifest 未改**（`bad.log`/`lightbad.log`/`ok.log`/`out_*__acsd_run_*.json`
  仍是原次运行的证据）；下表是改键后的复跑读数。

| 组 | 命令 | RC | `io_read` 命中 | 输出目录条目 / 帧产品 | 是否开工前失败 |
|---|---|---|---|---|---|
| 反例 E1a | `./build/linux-control/acsd normalize -y --json cfg_bad.json` | **3** | **0** | 7 / **0** | 是 |
| 反例 E1b | `./build/linux-control/acsd normalize -y --json cfg_lightbad.json` | **3** | **0** | 7 / **0** | 是 |
| 正例对照 | `./build/linux-control/acsd normalize -y --json cfg_ok.json` | **0** | **23** | 22 / 5 | 不误报 |

反例错误串（复跑 run manifest 的 `error.message`，与归档同判据）：

```
E1a: phase1 failed: node cal failed: prerun: master frame size mismatch: master 'master_bias'
     (…/masterBias_BIN-1_4096x4096.xisf) is 4096x4096 but master 'master_flat' …
E1b: phase1 failed: node cal failed: prerun: light vs master size mismatch: light
     '/tmp/e1check/masterFlat_bad_4096x4112.fits' is 4096x4112 but master 'master_bias' …
```
