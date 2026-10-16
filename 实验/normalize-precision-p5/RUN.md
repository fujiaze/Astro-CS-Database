# 阶段一 Normalize 球面参考精度生产接线 — 运行与验证记录（p5）

分支 `audit/acsd-science-docs-p5`；本轮提交 `7663e0c5a`（含 `cab84d79b`、`c90bc2749`、`513b7a663`、`7663e0c5a`）。
输入索引/哈希/构建哈希见 `evidence.json`（由脚本从磁盘实算，非手抄）。

## 1. 复现命令

```bash
# 发布构建（ACSD_DEV_MODE=OFF, Release）
cmake --build build/linux-sci -j 8
# 开发构建（ACSD_DEV_MODE=ON, Release + 诊断）
cmake --build build/linux-dev  -j 8

# 单帧真实验证（M42 T2/M4, 原始 FITS 头**无 WCS**）
./build/linux-sci/acsd normalize -y --json run/norm-precision-test.json   # exit=0
./build/linux-dev/acsd  normalize -y --json run/norm-precision-dev.json  # exit=0
```

## 2. 真实输入事实（不凭文件名猜测）

| 项 | 值 | 来源 |
|---|---|---|
| 亮帧 | `M42_M4_T2_flying_dutchman-20251212@033644-600S-H-alpha.fts`（4096×4096, BITPIX=16, BZERO=32768） | FITS 头实读 |
| FITS 头 WCS | **无** CTYPE/CRVAL/CRPIX/CD（31 帧无 WCS 普查中的 T2/M4 帧） | 头普查 |
| 初始指向 hint | OBJCTRA/OBJCTDEC + FOCALLEN=1877 + XPIXSZ=9（header_pointing） | p1_wcs.json |
| IPV 求解 | `ipv_solve_from_memory_with_callback_d` 解出：45 对内点、RMS 0.0922″、SIP order 3、CRVAL(84.2314, −6.3681) | `p1_wcs.json#wcs_source=ipv` |
| 母版 | bias/dark(600s)/flat(H-alpha) XISF, `master_units=normalized, master_scale=65535` | 配置 + 读数 |
| Gaia XP | `gaia/GaiaDR3SP`（20 xpsd） | 配置 |

**结论：原始头无 WCS 的帧经阶段一自带 IPV 实际解出，不是解算失败。**

## 3. 本轮新增科学产物（首次真实产出）

`<frame>/frame_science.json`（schema `acsd.normalize.frame-science/v1`, jsonschema 0 错误）：
- 64/64 控制点 `availability=available`（precision.status.accuracy=`approximate`，omitted=[master_uncertainty]）；
- `F_ref=1000 ADU`（`explicit_linear_flux`, `native_calibrated`）、逐点 `V_F`、`SNR_ref`、`support_area_sr`、`covariance_rank`、`condition_number`；
- `surface_transfer`：V_B（目标叶 C 对角）、T=V_B/V_F、单位 `adu^2/sr^2`、`sr^-2`；
- 帧摘要 `area_weighted_median_log_snr = 19.9989`；
- 256 个局部模型文件（每控制点 psf/covariance/drizzle_operator/support 四类，sha256 逐项登记）。

`p1_final.json` 新增 `frame_science_published=true` 与 `frame_science_sha256`。

## 3b. canonical 与 legacy 诊断量的区别（不互相冒充）

同一帧同一次运行：

| 量 | 值 | 定义 | 定位 |
|---|---|---|---|
| `p1_snr.json#frames[].snr_reference.snr_f` | 11.7638 | Horne 逐源 SNR_F 的参考轮廓值，分母通量 = 组内检出通量**中位数**（`reference_flux_source=group_median`） | legacy 诊断（旧 F0 路径） |
| `frame_science.json#precision.frame_summary.reference_snr` | 19.9989 | GLS 固定参考源（F_ref=1000 ADU, `explicit_linear_flux`）+ 常量背景 nuisance，支持面积加权 log SNR 中位数 | **canonical 稀疏参考精度** |

两者被估计量不同（分母参考通量、背景 nuisance、估计子空间都不同），**不可互换**；legacy 值保持原路径可查，canonical 值只由新 producer 生产。`p1_stack.json#bunit`、HiPS `signal/properties#BUNIT=ADU/sr` 与 sidecar `signal.linear_unit=adu/native_calibrated` 三者一致（本帧 photometry 通道缺席 ⇒ native_calibrated，无 α 换算）。

## 3c. HiPS/FITS 预览

- `frame_signal_preview.fits` / `frame_signal_preview.png`：`<frame>/signal/Norder9/Dir1370000/Npix1371955.fits` 的实读预览（512×512 float32 → float64 复制后写出；asinh/线性拉伸，1–99.5 百分位）。
- 读数：262144/262144 有限；min −1.64e13、max 9.41e14、mean 3.13e12 ADU/sr —— 与历史 E2E「58 ADU/px ÷ Ω≈2.08e-11 sr ≈ 2.8e12 ADU/sr」的量级自洽（本帧未施加测光 ⇒ native ADU 面亮度）。

## 3d. 默认路径（无 snr.reference）验证

同一硬件/同一帧、配置去掉 `snr.reference` 后重跑：`exit=0`，`frame_science.json`
**仍然发布**且 schema 0 错误 —— `precision.availability=unavailable` +
`reason="no snr.reference configured (canonical reference precision
unavailable; legacy F0 path stays a named diagnostic)"`，`controls=[]`、
`models=[]`；`reference.kind=unavailable` 且 `flux_unit=adu`、
`normalization=native_calibrated`（输入面真实状态，不伪造参考身份）。signal
HiPS 产品面不受影响 —— 即「缺参考不阻断合法 signal」按合同落地。

## 4. 开发/发布构建一致性（串行、独立目录、同配置）

| 对象 | 结果 |
|---|---|
| 科学文件（signal/support/variance/ivar tiles + models/ + frame_science.json） | 1005/1009 逐字节相同 |
| 4 个差异 | 均为子产品 `properties` 的 `hips_creation_date` |
| `frame_science.json` | 仅 `run_id`、`config_sha256` 不同（两次运行/配置本就不同） |
| 64/64 控制点 `V_F`、`SNR_ref` | **逐位相同**（max 相对差 0.0） |

注：捕获压平按 `(py,px,leaf)` 排序后才达到该一致性（排序前按 tid 压平使 GLS 求和顺序随帧内并行宽度变化，出现 2e-13 相对级差异）。

## 5. 回归

- 单元层 160 条：159 通过；唯一红项 `quantization.s12...` 在本轮改动前同样为红（预存在）。
- `eng/tests/conformance/normalize_frame_science_schema_test.py`：18/18 通过（夹具正例、生产 serializer 输出、10 项形状负例）。
- `eng/tests/integration/test_registry_code_parity.py`：失败集与改动前一致（12 项既有失败，0 新增）。

## 6. 阅读顺序（证据 → 代码）

1. `lib/include/acsd/normalize_data.h`（typed 对象）
2. `lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp`（GLS 两条精确路径 + 秩判定）
3. `lib/infrastructure/scheduler/src/normalize_precision.cpp`（config→typed 输入→算法库→staging）
4. `module_adapters.cpp` 的 `p1_op_drizzle`（捕获 + staging）与 `p1_op_writer`（发布）
5. `lib/algorithms/integration/normalize_science/`（sidecar 唯一 serializer/reader + 发布）

## 7. ROUND4：参考通量总通量口径订正（NOISE_SNR §2）

本轮把空间 α 从"只进源泊松分母"改为"模板与源泊松共用同一 α 合同"
（`h_j=(α_j/α_*)P_j`、`Var_src=F_ref·α_j²·P_j/(g·α_*)`，`F_ref(u)=α_*·F_native`），
删去旧口径的窗口 sum 归一。推导、实现点、独立闭式/MC 判据与真实产品 A/B 数字见
`ROUND4_FLUX_CONVENTION.md`；日志见 `evidence/verify_round4_flux_total.log`、
`evidence/run_p5f_flux_total.log`、`evidence/verify_round6_alpha_contract.log`。
