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
