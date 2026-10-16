# Issue #11 验证报告：Mosaic 名义通带身份（修复 `6286e659`）

- Issue: [#11](https://github.com/fujiaze/Astro-CS-Database/issues/11)（任务书评论 `6220357531`）
- 验证基线/报告 main: `ece85ace5c3f7151f6afbd0ee1aa17abf7bedb21`（`git fetch` 后的实时 origin/main）
- 被验证的静态修复: `6286e659f`（`lib/infrastructure/scheduler/src/module_adapters.cpp::p2_op_write`
  通带分类改用具名 `have_first`，不再用 `first.empty()` 推断首帧）
- **本轮零生产代码修改**：仅新增 `eng/tests/**`（如需）与 `实验/issue11-passband-identity/**`。
  §6 的 pre 二进制由**未提交工作区临时还原**修复后构建、随即 `git checkout` 恢复并重建
  （`code/build_ab_pre11.sh`；恢复后 `git diff` 为空）。未改 `docs/**`、未做 #12/ARS。

## 1. 结论

名义通带声明已成为输入 `filter_passband` **多重集**的对称函数：
`["","Halpha"]` 与 `["Halpha",""]` 都得 `obs_filter=mixed:2_passbands`；
全空 ⇒ 空（undeclared）；全同非空 ⇒ 原名；≥2 种（含空+非空）⇒ `mixed:<n>_passbands`。
**12/12 真实 CLI 用例 exit 0 并发布完整产品**；同多重集 6 种排列的
`signal/properties:obs_filter` 与 `p2_final.json:response_distribution` **逐字相同**；
pre/post 同配置 **signal 185/185 tile 像素逐元素相同**。
下游 Phase3/Export **不读取**该名义身份（发现，未修）。

## 2. 构建记录

| 项 | 值 |
| --- | --- |
| 命令 | `ninja -C build/linux-sci-p5`（default all，生产 CLI `acsd` + 全部生产库） |
| 平台 | Linux 6.12.107+deb13 x86_64；g++ 14.2.0；CMake 3.31.6；ninja 1.12.1；16 vCPU |
| 结果 | exit 0（`build/01-build-post.log`） |
| post 二进制 | sha256 `9175527877afe054ca9b5ff89f4f41fc67dc2326e5ec9eb23dd43e4120457636` |
| pre-#11 二进制（临时还原） | sha256 `93a6be342bb8bcdd275d05265661045c7a37760e569d40e8b01427a18dbc6772` |

## 3. 字段路径（实际生产）

```
输入 Phase1 HiPS signal/properties:obs_filter
  └─ coverage inspect_frame → 每帧 filter_passband → p2_coverage.json#frames[]
      └─ p2_op_write 分类（counts / have_first）
          ├─ aio_hips_product_begin(obs_filter) → 最终 HiPS signal/properties:obs_filter
          │                                      + 每个叶 tile FITS 头 FILTER 键（非空时）
          └─ p2_final.json:obs_filter / response_distribution
下游 Phase3 Export：p3_op_properties 只读 signal/properties 的 BUNIT/hips_order；
                    **不读 obs_filter / response_distribution / p2_final.json**（§9）。
```

## 4. 真实 CLI E2E 矩阵（12/12 PASS，`evidence/20-post-matrix.json`）

真实输入 = 仓内真实 Phase1 HiPS 产品的**硬链接 overlay**（科学数据逐字节不变），
仅把每帧 `signal/properties:obs_filter` 名义标签改成测试值；`acsd mosaic --json cfg -y`。

| 用例 | 输入标签（按帧序） | exit | `properties:obs_filter` | tile FITS `FILTER` | `response_distribution` | 判定 |
| --- | --- | --- | --- | --- | --- | --- |
| empty_halpha | `["","Halpha"]` | 0 | mixed:2_passbands | mixed:2_passbands | (undeclared):1, Halpha:1 | PASS |
| halpha_empty | `["Halpha",""]` | 0 | mixed:2_passbands | mixed:2_passbands | (undeclared):1, Halpha:1 | PASS |
| empty_empty | `["",""]` | 0 | `""` | （无 FILTER 键） | (undeclared):2 | PASS |
| red_red | `["Red","Red"]` | 0 | Red | Red | Red:2 | PASS |
| red_blue | `["Red","Blue"]` | 0 | mixed:2_passbands | mixed:2_passbands | Blue:1, Red:1 | PASS |
| blue_red | `["Blue","Red"]` | 0 | mixed:2_passbands | mixed:2_passbands | Blue:1, Red:1 | PASS |
| pem_01..06 | `["","Red","Blue"]` 六排列 | 0 | mixed:3_passbands | mixed:3_passbands | (undeclared):1, Blue:1, Red:1 | PASS |

每例都发布完整 `p2_final.json` + signal/support/variance/ivar HiPS（188/189 个 signal 文件），
run manifest `status=ok`。空标签时叶 tile 无 `FILTER` 键（AIO 仅在非空时写），与"未声明"一致。

## 5. 排列不变性（逐字）

`code/compare_permutation_meta.py`（`evidence/26-permutation-metadata.json`）：

| 组 | n | `obs_filter` 行集合 | `response_distribution` 规范串集合 | 判定 |
| --- | --- | --- | --- | --- |
| empty+Halpha | 2 | {`obs_filter=mixed:2_passbands`} | {`{(undeclared):1,Halpha:1}`} | PASS |
| Red+Blue | 2 | {`obs_filter=mixed:2_passbands`} | {`{Blue:1,Red:1}`} | PASS |
| empty+Red+Blue | 6 | {`obs_filter=mixed:3_passbands`} | {`{(undeclared):1,Blue:1,Red:1}`} | PASS |

（跨排列仅 `phase2_audit` 的**浮点**字段有末位差——天空面联合解的归约顺序，属任务书
预期；名义身份不受影响。）

## 6. pre/post #11 同配置 A/B（隔离修复）

同配置 = `["","Halpha"]`（2 帧真实输入、同一 overlay 内容、同一标签序列）：

| 面 | pre-#11 | post-#11 |
| --- | --- | --- |
| `properties:obs_filter` | **Halpha**（错：空首帧被后续已知帧覆盖基准） | **mixed:2_passbands** |
| `p2_final.json:obs_filter` | Halpha | mixed:2_passbands |
| `response_distribution` | (undeclared):1, Halpha:1 | (undeclared):1, Halpha:1（相同） |
| 反序 `["Halpha",""]` | mixed:2_passbands | mixed:2_passbands |

⇒ pre **随输入次序改变身份**（Halpha vs mixed:2）；post 两序一致（§5）。这正是修复目标。
`response_distribution` 在 pre 已正确；pre 唯一错的是 `obs_filter` 选择（与 Issue 描述一致）。

## 7. 科学数值不变性（`evidence/23-AB-compare.json`）

pre/post 同配置产品对照：

| 面 | 结果 |
| --- | --- |
| **signal 叶 tile DATA 数组** | **185/185 逐元素相同**（0 差异 tile） |
| 二进制科学面 | `p2_integrated_signal/support/wsum/nused/nrej.bin`、`p2_sky_plane.bin`、`p2_samples.json`、`p2_rejection_*.bin` 等 sha256 **相同** |
| `p2_final.json` | 仅 `obs_filter`（预期）与**绝对路径**字段（`hips_root`/`properties`/artifact）不同；科学数值字段相同 |
| `p2_integrated.json`/`p2_corrected.json`/`p2_rejection.json` | 差异**仅绝对路径**（pre/post 输出目录与 overlay 根不同），无数值/哈希差异 |

## 8. 负例：显式空串 vs 缺少 `obs_filter` key（`evidence/22-*`）

- 显式空串 `obs_filter=`（key 存在）⇒ 合法"已声明 unknown"，coverage 通过（§4）。
- **删除** `obs_filter` 行 ⇒ coverage `inspect_frame` fail-closed：
  `missing obs_filter property (filter/passband identity is required for compatibility)`，
  CLI **exit 3**（INPUT），**不发布任何产品**。两者不混称。**PASS**。

## 9. 下游 Export 消费面（真实发现，未修）

对完整 mosaic 产品跑真实 `acsd export`（exit 0，产出 `output_phase3.fits`）：

- `p3_props.json` **无**任何 filter/response 键（仅有 bunit/hips_order/pixel_semantics 等）；
- `output_phase3.fits` 头 **无** `FILTER`/`RESP*` 键；
- 静态检索：`lib/algorithms/{resample,projection,fits_output}` 与 phase3 节点**无**
  `obs_filter`/`response_distribution`/`p2_final.json` 消费者。

⇒ **Phase3/Export 不携带名义通带身份**。因此"P3 消费"验收层级**不存在可测消费者**
（`NOT_RUN/ABSENT`，非失败），导出 FITS 的标称通带需由前台/Codex 定约（未擅自扩张）。

## 10. 异常（只上报，不修）

**3 帧异响应联合天空面不解**：用同一 M42_M1 天区的 2 帧 H-alpha(600s) + 1 帧 Red(300s)
（不同滤镜/曝光）时，`upm_fit` 报
`surface build FAILED rc=6 adaptive node spacing exhausted: … reduced normal matrix not SPD`
⇒ `upm_apply` 缺 `p2_sky_plane.bin`，CLI exit 2（`evidence/25-anomaly-3frame-mixed-response.stderr`）。
该失败与 #11 标签修复无关（标签不参与天空面数值）；但说明"合法混通带输入"在 3 帧异响应下
当前**无法到达 writer**。3 帧标签排列用例改用同响应（同 M42_M2 面板、同滤镜/曝光）三帧后才 E2E 通过。

## 11. PASS / FAIL / NOT_RUN 分级

| 验收层级 | 状态 | 证据 |
| --- | --- | --- |
| 静态已修（`have_first`） | **PASS** | `evidence/01-fix-6286e659.diff` |
| 生产 CLI 构建 | **PASS**（exit 0） | `build/01-build-post.log` |
| 真实 CLI 可到达 writer + 完整产品发布（12 例） | **PASS**（exit 0，188/189 文件） | §4 |
| 最终 HiPS `properties:obs_filter` + tile `FILTER` | **PASS** | §4 |
| `p2_final.json:response_distribution` | **PASS** | §4/§5 |
| 排列不变性（元数据逐字） | **PASS** | §5 |
| pre/post 科学数值不变 | **PASS** | §7 |
| 负例（空串 vs 缺 key） | **PASS** | §8 |
| P3/Export 名义身份消费 | **NOT_RUN / ABSENT**（无消费者） | §9 |
| 3 帧异响应到达 writer | **FAIL（独立异常，未修）** | §10 |

## 12. 复跑命令

```bash
# 构建
ninja -C build/linux-sci-p5
# pre-#11 A/B 二进制（临时还原→构建→恢复→重建）
bash 实验/issue11-passband-identity/code/build_ab_pre11.sh
# 受控输入 overlay（真实产品硬链接，仅改 obs_filter 名义标签）
python3 实验/issue11-passband-identity/code/make_labeled_inputs.py --src <real> --dst <ovl> --label <L>
# 12 例真实 CLI 矩阵（post）
python3 实验/issue11-passband-identity/code/run_passband_matrix.py \
    --acsd /tmp/issue11_acsd_post --out /workspace/issue11-scratch/matrix_post \
    --keep empty_halpha --tag post
# 负例 + Export 消费面
python3 实验/issue11-passband-identity/code/run_negative_and_export.py \
    --acsd /tmp/issue11_acsd_post --product <complete_mosaic_dir> --work <scratch>
# 排列元数据逐字对照 / pre-post 科学数值对照
python3 实验/issue11-passband-identity/code/compare_permutation_meta.py --meta <matrix>/meta
python3 实验/issue11-passband-identity/code/compare_mosaic_products.py <pre> <post>
```

## 13. 交付边界

- 生产源码**零修改**；未改 docs；未做 #12/ARS。仅新增实验单元
  `实验/issue11-passband-identity/{code,evidence,build}`（大产品/真实 FITS 留在
  `/workspace/issue11-scratch`，不入 Git）。
- 受控输入说明：真实 Phase1 产品的硬链接 overlay，**只改被测量本身**
  （名义 `obs_filter`），不伪造 integrated/coverage 上游产物。
