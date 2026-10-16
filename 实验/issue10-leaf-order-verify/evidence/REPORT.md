# Issue #10 验证报告：`frame_science.signal.leaf_order` 真实生产核对

- Issue: [#10](https://github.com/fujiaze/Astro-CS-Database/issues/10)（施工评论 `6218578949`）
- 起始/验证基线 main: `0793b8d621bcdc98485d14994f031502c467a71c`（任务评论写定值）
- 报告 main: `4da73ed5c67b4de27c7f91635eafae911d2c38ca`（在并发的无关 P2 修复 `b2c82f7ae`
  之上 rebase；`git diff 0793b8d62 HEAD -- module_adapters.cpp normalize_science/` 为空 ⇒
  本报告结论在报告 main 上同样成立）
- 被验证的静态修复: `e8ff4bc90`（`module_adapters.cpp::p1_op_drizzle`：`leaf_order` 由 `k-9` → `k`）
- **本轮零生产代码修改**（仅新增 `eng/tests/**` 测试与 `实验/issue10-leaf-order-verify/**` 脚本/证据）。
  §5 的 A/B 需要"修复前同一配置"二进制，做法是在**未提交的工作区**临时把该行还原、构建后立即
  `git checkout` 恢复并重建（`code/build_ab_pre10.sh` 记录全过程；`git diff` 恢复后为空；见 §5）。

## 1. 结论（一句话）

`signal.leaf_order` 现为**真实 HEALPix 叶阶 K=log2(frame_nside)**：真实 CLI 产品
K=18 → `signal.leaf_order=18`，而 `signal/properties:hips_order=9`、磁盘叶目录 `Norder9`
（= K−9）、`hips_pixel_scale`、controls `leaf_order=18`、Ω=4π/(12·4^K) 六个独立面互相一致。
与 pre-#10 同配置 A/B 对照：**187/187 signal FITS tile 逐字节相同、全部 64 个控制点 precision/SNR 相同**，
sidecar 仅 `signal.leaf_order`（9→18）与时间戳/run 哈希变化。读者兼容性有 1 个真实风险（§7，不修）。

## 2. 构建记录

| 项 | 值 |
| --- | --- |
| 命令 | `ninja -C build/linux-sci-p5`（default all；生产 CLI `acsd` + 全部生产库） |
| 平台 | Linux 6.12.107+deb13-amd64 x86_64；g++ (Debian 14.2.0-19) 14.2.0；CMake 3.31.6；ninja 1.12.1；16 vCPU |
| 构建目录 | `build/linux-sci-p5`（Release，`ACSD_ENABLE_SANITIZERS=OFF`） |
| git main | `0793b8d621bcdc98485d14994f031502c467a71c`（工作树仅新增本实验目录） |
| 结果 | `[19/19] Linking CXX executable acsd`，**exit 0**（`build/01-build-post.log`） |
| post 二进制 sha256 | `16b8621a912bb3b1fdfbeda9b29b8f9c3d030cb3bfbd8500680745e77d7a67b2` |
| pre-#10 A/B 二进制 sha256 | `66b3df4e33a8edab17fc31e3d2104a1691a5dc8da8086c1a0d4b5e88b1d7f2ed` |

**构建回归**：`0793b8d62`（Codex ABI 文档提交）与 `#11/#12/#45/#56/#57/#61/#52/#53` 之后的
全量生产目标增量重编译通过，无构建错误。

> 观察（非本轮范围）：二进制内嵌版本戳 `ACSD_COMMIT_SHA=015a18b2…` 是**陈旧值**（未重跑
> `cmake` configure，`version_generated.h` 不随 HEAD 更新）⇒ `frame_science.software_sha`
> 对 pre/post 两个本地构建**相同**，不能作为"旧格式 vs 新格式"的判别依据（§7）。

## 3. 真实 E2E：Drizzle → Writer → `frame_science.json` → Reader

- 配置：`code/run_e2e_matrix.py` 生成的单块 normalize（真实 testdata M42 T2 M4 H-alpha +
  T2 母版 + `gaia/GaiaDR3SP` + 显式参考通量），命令 `acsd normalize --json <cfg> -y`。
- `auto`（`drizzle.nside=0`，复现历史 K=18 构型）：**exit 0，wall 292.29 s**；
  发布 `out/<frame_key>/frame_science.json` + `signal/`（187 FITS + properties）。
- 真实 `read_frame_science_document` 回读：`ok=true`、`findings` 为空、`signal.leaf_order=18`、
  64 控制点 `leaf_order=18`（`eng/tests/unit/frame_science_leaf_order_reader_test.cpp`）。

## 4. 独立 oracle 核对（不把 sidecar 声明自己当 oracle）

`code/verify_leaf_order.py`：K=18 由**三个独立来源**同时确定（properties.hips_order+9、
controls[*].leaf_order、CLI 构型），再逐项核对 → **14/14 PASS**（`evidence/04-verify-post-auto.json`）：

| 核对 | 结果 |
| --- | --- |
| `signal.leaf_order == K(18)` | PASS |
| `signal.leaf_order != K-9`（bug 签名） | PASS（=18 ≠ 9） |
| `controls[*].leaf_order == 18`（64/64） | PASS |
| `controls[*].leaf_ipix < 12·4^18 = 824633720832` | PASS |
| `signal/properties:hips_order == K-9 = 9` | PASS |
| signal 叶目录 `Norder == K-9 = 9`（磁盘实际路径） | PASS |
| `hips_tile_width == 512` | PASS |
| Ω(K)=4π/(12·4^K) == `(hips_pixel_scale → rad)²` | PASS（rel 3.0e-3，受 properties 6 位有效数字限制） |
| `support_area_sr == N·Ω`（正整数倍） | PASS（N∈[785,798]，worst 8.5e-9） |

真实物理量例：`frame_nside=2^18`；叶数 `12·4^18=824633720832`；单叶面积 `Ω=1.523873e-11 sr`；
properties `hips_pixel_scale=0.000224°` ⇔ nside 2^18；磁盘叶目录 `Norder9`。

## 5. 科学数值回归对照（pre-#10 vs post-#10，同配置）

- A/B 二进制（§2）：`acsd_pre10`（临时还原 `leaf_order=k-9`）与 `acsd_post`（现行 main）。
- 同一 auto 配置各跑一次真实 CLI（`auto_pre10` exit 0 / 201.08 s；`auto` exit 0 / 292.29 s）。
- `code/compare_products.py` → `evidence/11-AB-compare.json`：

| 面 | 结果 |
| --- | --- |
| **signal FITS tile** | **187/187 逐文件 sha256 相同**（0 changed / 0 only-pre / 0 only-post） |
| signal `properties` | 仅 `hips_creation_date` 时间戳不同（其余逐键相同，含 `hips_order=9`） |
| **precision.controls** | **64/64 全字段相同**（reference_snr / flux_variance / support_area_sr / leaf_order / leaf_ipix / covariance_rank / condition_number） |
| frame_summary | 相同（reference_snr / supported_area_sr / n_valid） |
| sidecar 差异 | 仅 3 项：`signal.leaf_order` 9→18、`run_id`、`config_sha256`（两 config 的 output_dir 不同所致） |

即：**除预期的 `signal.leaf_order` 元数据订正外，Signal 像素与 SNR/精度数值零变化**。
sidecar 文件 sha256 变化（pre `acea6fa3…` → post `26c32c5b…`）属预期，不用于判数值回归。

## 6. K=9 / K=11 / K=13 数值例子

数学 oracle（`frame_science_leaf_order_reader_test.cpp` §math，不分配任何帧）：

| K=log2(nside) | nside | `signal.leaf_order` | tile/Norder=K−9 | Ω=4π/(12·4^K) |
| --- | --- | --- | --- | --- |
| 9 | 512 | 9 | 0 | 3.994742e-06 sr |
| 11 | 2048 | 11 | 2 | 2.496714e-07 sr |
| 13 | 8192 | 13 | 4 | 1.560446e-08 sr |

K−9 与 Ω 公式断言 **PASS**；`4^9 = 262144`（把 tile 阶当叶阶时面积差 262144 倍）。

真实 CLI 对显式粗 nside 的补充证据（产品写侧已完成部分，见 §8 异常）：

| 构型 | properties `hips_order` | 磁盘叶 `Norder` | `hips_pixel_scale` |
| --- | --- | --- | --- |
| `nside=512` (K=9) | 0 = 9−9 ✔ | `Norder0` ✔ | 0.114516° ✔ |
| `nside=2048` (K=11) | 2 = 11−9 ✔ | `Norder2`（另有 0/1 hierarchy）✔ | 0.028629° ✔ |

## 7. 读者兼容性（真实发现；**未**实现任何迁移）

用真实 `read_frame_science_document` 探针（`evidence/07-reader-test.log`）与仓内真实旧产品
（`code/scan_legacy_sidecars.py`，`evidence/03-legacy-sidecar-scan.json`）：

1. **旧格式被静默接受**：仓内 31 个历史 sidecar 中 **29 个**带 bug 签名
   `signal.leaf_order == K−9`（K=18 → 值 9），其中 **10 个已发布产品**（`run/*/out/...`）。
   真实样本 `run/norm-precision-test/out/…/frame_science.json`（`software_sha=015a18b2`）：
   `signal.leaf_order=9`，而真实叶阶（controls + properties + Norder + pixel_scale）为 **18**。
   当前 reader 回读 `ok=true`、`findings` 为空、值原样为 9。
2. **运行时零校验**：`signal.leaf_order` 无任何运行时约束——探针 `=0`、`=-5` 同样被
   `ok=true / findings=0` 接受（JSON schema `minimum: 9` 只在 conformance 测试里对 schema 生效，
   reader 不加载 schema）。
3. **无可判别版本**：新旧 sidecar `schema_id` 均为 `acsd.normalize.frame-science/v1`、
   `schema_version=1`（**未升版**）；`software_sha` 在本地增量构建下是陈旧常量（§2）。
   ⇒ **无法**据 schema/产品版本可靠鉴别旧格式（符合施工评论"不能可靠鉴别则保留具名诊断并报告"）。
4. **受影响消费者**：全仓检索 `signal.leaf_order` 的**唯一运行时消费者**是
   `read_frame_science_document` → `FrameScienceDocument.signal.leaf_order` 字段本身；
   当前 C++ 生产链**没有任何数值/几何消费者**（grep 证据），故危害目前是"元数据被静默错读"的潜在风险，
   不是已发生的数值错误。Python 侧仅 conformance schema 测试按 type/minimum 校验。

## 8. 异常与独立发现（只上报，不扩展修复）

1. **显式粗 nside 的 E2E 停滞（NEW，与 #10 无关）**：`drizzle.nside=512` 与 `=2048` 两个真实
   CLI 构型都在 **per-frame HiPS 写出之后**进入单线程用户态计算循环（热点线程 ≈1.05 core），
   `frame_science.json` 不产出，长时间不退出（k9 1204.78 s、k11 236.91 s 后被 kill）；
   同配置 `nside=0`（2^18）292 s 正常完成。最短复现 = `evidence/05-e2e-matrix.json` 的
   `k9`/`k11` 两条（config 保留在 `/tmp/issue10/run/{k9,k11}/config.json`）。
   **状态：NOT_RUN / ANOMALY，根因未定位**（超出本轮"只验证 #10"的范围）。见 `evidence/08-k9-anomaly.txt`。
2. **版本戳陈旧（NEW）**：`ACSD_COMMIT_SHA` 在增量构建下不更新 ⇒ `software_sha` 不能标识产出代码
   （也削弱"用版本鉴别旧格式"的可能）。
3. **schema 未升版 + reader 零校验（§7）**：同一 `schema_id/v1` 下字段语义已改变，reader 不校验
   ⇒ 旧产品被静默当新 leaf order 读取。仅报告，不擅自实现 `+9` 迁移（按施工评论）。

## 9. PASS / FAIL / NOT_RUN

| 项 | 状态 | 证据 |
| --- | --- | --- |
| main 完整编译生产目标 | **PASS** | `build/01-build-post.log`（exit 0） |
| Drizzle→Writer→frame_science.json 发布（auto K=18） | **PASS** | `/tmp/issue10/run/auto`（exit 0） |
| 真实 reader 回读同产品 | **PASS** | `evidence/07-reader-test.log` |
| 独立 oracle（K/Norder/Ω/controls） | **PASS 14/14** | `evidence/04-verify-post-auto.json` |
| pre/post 科学数值无回归 | **PASS** | `evidence/11-AB-compare.json`（187/187 FITS 同） |
| K=9/11/13 数学例 | **PASS** | `07-reader-test.log` math 段 |
| K=9/K=11 真实 CLI 科学 E2E | **NOT_RUN（停滞异常）** | `evidence/08-k9-anomaly.txt` |
| 旧格式兼容（静默接受） | **PASS（发现，非修复）** | `evidence/03/07/12` |
| 真实 testdata E2E（M42 重跑） | **PASS** | §3（使用仓内真实 M42 T2 M4） |

## 10. 复跑命令

```bash
# 1) 构建（记录 SHA/平台）
ninja -C build/linux-sci-p5
# 2) A/B pre-#10 二进制（临时还原→构建→恢复→重建；main 树不变）
bash 实验/issue10-leaf-order-verify/code/build_ab_pre10.sh
# 3) 真实 CLI E2E 矩阵（auto post / k9 / k11 / auto pre10）
python3 实验/issue10-leaf-order-verify/code/run_e2e_matrix.py \
  --acsd-post /tmp/issue10/bin/acsd_post --acsd-pre10 /tmp/issue10/bin/acsd_pre10 \
  --cases auto,k9,k11 --base /tmp/issue10/run
# 4) 独立 oracle / pre-post 对比 / 旧产品扫描
python3 实验/issue10-leaf-order-verify/code/verify_leaf_order.py <product> --expect-K 18
python3 实验/issue10-leaf-order-verify/code/compare_products.py <pre> <post>
python3 实验/issue10-leaf-order-verify/code/scan_legacy_sidecars.py --root run
# 5) 真实 reader 测试
python3 eng/tests/unit/run_frame_science_leaf_order_reader_test.py <product> 18 [legacy.json]
```

## 11. 交付边界

- 生产源码**零修改**；`docs/**`、`AGENTS.md` 未动。仅新增 `eng/tests/unit/` 两个文件与本实验单元。
- 未实现任何旧格式自动迁移；未改 #11/#12/ARS 范围。
- 真实观测 FITS 与大产品留在 `/tmp`，不入 Git；仓内只提交轻量 sidecar/properties/哈希/日志/脚本。
