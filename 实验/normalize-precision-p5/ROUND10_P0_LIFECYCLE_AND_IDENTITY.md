# ROUND10：Drizzle 帧生命周期（P0 UAF + 泄漏）、测光身份规范串化、F_syn 光子率 SI 订正

对应 Issue #7（A/B/C）与 Issue #5（A/B）。三部分都是**源码/测试/实验**改动，
科学定义的正式表述交 Codex（见 §4 交接）。

## 1. P0：`p1_op_drizzle` 帧生命周期

### 1.1 缺陷（两个，同类所有权问题）

静态基线 = `run/P0-EXPTIME/` 复现时的 `main=b1f4fd72e`。

| # | 位置（修复前） | 类型 | 触发条件 |
|---|---|---|---|
| E07-UAF | `module_adapters.cpp:9500` | **use-after-free 读** | `hp_drizzle_run_phase1_hips` 返回后 `aio_pipeline_frame_destroy(frame)`（:9442）已释放帧；随后 precision staging 在**输入亮帧头 EXPTIME 无效**时调用 `aio_frame_kv_get(frame, "header", "EXPTIME")` |
| E07-LEAK | `module_adapters.cpp:9206` | **对象泄漏** | SCI-NOISE-001 §5d 方差面可审计性 fail-closed 分支 `return;` 前未 `aio_pipeline_frame_destroy(frame)`（该帧的块与像素缓冲永不释放） |

### 1.2 完整所有权审计（不是只改一行）

对 `p1_op_drizzle`（`frame = aio_pipeline_frame_create()` 于 8717 行起）逐条枚举
destroy 与 return：

| 行（修复前） | destroy | 其后是否触碰 frame | 结论 |
|---|---|---|---|
| 8721 | — | `return`（frame==nullptr，创建失败分支） | 安全（对象不存在） |
| 8751 / 8811 / 8826 / 8835 / 8949 / 9123 / 9219 | ✅ | 紧接 `return` | 安全（终止路径） |
| 9206 | ❌ | 紧接 `return` | **泄漏（E07-LEAK）** |
| 9442 | ✅ | **9500 行再读 frame header KV** | **UAF（E07-UAF）** |
| 9455 / 9723 / 9791 / 9816 / 9818 / 9832 | 已在 9442 释放 | 只用字符串/局部量 | 安全（9500 修好后无其它引用） |

其它可能被误认为"帧内存别名"的量已核实不越界：`db`（`aio_frame_get_block`）作用域
限于 dtype 检查块；`px_ptr`/`nm_data`/`var_plane` 在 destroy 之后零引用；`im` 是
`p1_read_image()` 独立读取的对象。⇒ 全函数**只有**上述两处缺陷。

### 1.3 修复（最小且可证明，不引入第二份 I/O/全局态）

```cpp
// destroy 之前一次性取出所需元数据（frame 仍合法）
double frame_header_exptime = 0.0;
{
  const char* expt = aio_frame_kv_get(frame, "header", "EXPTIME");
  if (expt != nullptr && expt[0] != '\0') {
    const double v = std::atof(expt);
    if (std::isfinite(v) && v > 0.0) frame_header_exptime = v;
  }
}
aio_pipeline_frame_destroy(frame);
frame = nullptr;            // 显式失效：destroy 后任何再触碰都是空指针而不是静默 UAF
```
precision staging 改用局部 `frame_header_exptime`（**优先级/语义不变**：输入亮帧头
EXPTIME 优先，其次 frame header EXPTIME）。§5d fail-closed 路径补
`aio_pipeline_frame_destroy(frame); frame = nullptr;`。

**为什么不做函数级 RAII 重构**：destroy 位于 drizzle 之后、precision staging 之前是
有意的——4096² FP64 帧块约 134–268 MB，保持"用完立即释放"的内存峰值语义；改成
作用域 RAII 会把该块一直持有到函数末尾（precision staging 是重计算段）。故选择
"在释放点前取出元数据 + 显式置空"，并把该规则写成注释固定在释放点。

### 1.4 可达性分析与最小反例

风险分支要求：`preq_ok && rc == 0`（有可用 WCS 产物 + drizzle 成功）**且**
`aio_read_metadata(lp).calibration.exptime` 无效。两个相邻机制会先拦住配置路径：

1. **IPV 极限星等**需要曝光时间（`m0` 由 `t` 派生）⇒ 亮帧头无 EXPTIME 时解不出 WCS，
   `preq_ok=false`，precision staging 整段跳过。
2. **暗场在位时 K=t_light/t_dark 必须由亮帧 EXPTIME 推导**（DATA fail-closed）⇒ 有
   暗场就轮不到 precision staging。

⇒ 最小反例 = **显式 WCS（`wcs.init_source=explicit_config`，取真实解）+ 偏置/平场
（无暗场）+ 无 EXPTIME 卡**的亮帧。复现脚本与输入：

```bash
# 1) 造 5 个 EXPTIME 变体（真实帧仅改头卡 80 字节）
python3 实验/normalize-precision-p5/code/make_exptime_variants.py <real.fts> run/P0-EXPTIME/inputs
# 2) 真实生产入口（-force = 显式越过"缺母版暗场"的预检; 无暗场是本反例的前提）
./build/asan/acsd normalize -y -force --json run/P0-EXPTIME/wcscfg-missing.json
```

### 1.5 证据（前后对照）

**(a) 修复前 · ASan**（`evidence/p0_exptime_asan_prefix_missing.log`，构建
`-DACSD_ENABLE_SANITIZERS=ON`：ASan+UBSan）：

```
==1847466==ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 4
    #0 find_block_index            lib/infrastructure/aio/src/aio_pipeline.cpp:390
    #1 aio_frame_kv_get            lib/infrastructure/aio/src/aio_pipeline.cpp:806
    #2 operator()                  lib/infrastructure/scheduler/src/module_adapters.cpp:9500
    #3 p1_parallel_for<...p1_op_drizzle...>   .../module_adapters.cpp:2303
    #4 p1_op_drizzle               .../module_adapters.cpp:8587
Address ... is a wild pointer inside of access range of size 0x000000000004.
SUMMARY: AddressSanitizer: heap-buffer-overflow ... in find_block_index
EXIT=1
```
（ASan 归类为 heap-buffer-overflow 而非 heap-use-after-free：被释放的帧内存已被
复用/落入红区，故命中的是红区地址；栈帧第 #2 行就是本次修复的那一行。）

**(b) 修复后 · ASan**（同配置、同输入）：`AddressSanitizer/runtime error` 命中 **0**，
进程 exit=0，sidecar 发布：

```
[writer][precision] frame_science.json published: .../frame_science.json (sha256=e716fd02...)
exposure_seconds = 0.0
exposure_source  = unavailable: no EXPTIME in input light header
n_controls = 64 (64 available)  frame_snr = 14.471222195983142
```

**(c) 修复后 · release 五变体矩阵**（`code/run_p0_exptime_matrix.py`，
`evidence/p0_exptime_matrix_fixed.log` + `p0_exptime_fixed_matrix.json`）：

| 变体 | exit | sanitizer | exposure_seconds | exposure_source | controls | frame_snr |
|---|---|---|---|---|---|---|
| valid（600 s） | 0 | — | 600.0 | input light header EXPTIME | 64/64 | 14.471222195983142 |
| missing | 0 | — | 0.0 | unavailable: no EXPTIME in input light header | 64/64 | 14.471222195983142 |
| zero | 0 | — | 0.0 | 同上 | 64/64 | 14.471222195983142 |
| negative | 0 | — | 0.0 | 同上 | 64/64 | 14.471222195983142 |
| nan | 0 | — | 0.0 | 同上 | 64/64 | 14.471222195983142 |

⇒ 非法 EXPTIME **不进入** free 内存、不崩溃、不给假数值，而是具名 unavailable；且五
变体的 signal/sidecar 科学构型（64/64 控制点、帧摘要 SNR）逐位一致（EXPTIME 不参与
precision 数值）。

**(d) 生产回归（正常构型）**：`run/norm-precision-test.json` 用修复后二进制重跑
（`evidence/run_p5h_regression_after_fix.log`，exit=0）：帧摘要
`reference_snr = 14.030187080046197` 与修复前**逐位一致**，`64/64` 控制点。
（该帧未施加测光 ⇒ `scale_applied=false`、`application_id=null`，见 §2.4。）

## 2. 测光身份：无损规范串化

### 2.1 缺陷与实测

修复前 `application_id` 的规范串用 `std::to_string(frame_photscal)`。C++17
`std::to_string(double)` 只有 6 位小数：

```
std::to_string(1.524215866973e-17) = "0.000000"
std::to_string(1.524315866973e-17) = "0.000000"   // 两个不同标度 → 同一身份
```

### 2.2 规范（生产 API = `acsd::phase1::precision`）

* `canonical_double(v)`：`std::to_chars(..., std::chars_format::hex)` 的 **hexfloat**
  （IEEE-754 值 ↔ 文本一一对应；`strtod("0x"+text)` 逐位回读；不经过 locale）。
* `photo_application_identity(AppliedScaleIdentity)`：规范串覆盖
  `frame_key | 输入 occurrence 内容 sha256 | k_photo | 空间场(order/status/
  identifiable/coef/center/norm) | 滤镜键+内容 sha256 | QE 键+内容 sha256`，
  输出 `photoapplied:v2:<sha256(规范串)[0:32]>`（**v2 版本前缀**是本规范的兼容把手）。
* `photo_response_identity(...)`：`<filter_key>@<sha12>+<qe_key>@<sha12>`；
  内容摘要缺失写 `@none`（不写假摘要）；未装载任何曲线 ⇒ 空串（不虚构身份）。
* 曲线内容摘要由 `file_sha256_hex(curve_source)` 在**逐帧循环外**算一次（不逐帧 I/O）。

### 2.3 独立单元测试（`eng/tests/unit/photo_identity_test.cpp`）

`python3 eng/tests/unit/run_photo_identity_test.py` ⇒ **ALL PASS**（日志
`evidence/photo_identity_test.log`）：

| 判据 | 实测 |
|---|---|
| `canonical_double` 无损（13 个值：1e-17 邻域、±0、denormal 5e-324、1e308、1/3…） | hexfloat→strtod 逐位相同 |
| `canonical_double` 单射（不同 binary64 ⇒ 不同串） | 全对通过 |
| 两个相邻 ~1e-17 标度串化不同 | `1.192b0f2999925p-56` ≠ `1.192b0f2fc8169p-56` |
| locale 无关（自定义 `numpunct` 逗号小数分隔符 + `LC_NUMERIC`） | 输出不变、不含 `,` |
| 同输入 ⇒ 同 ID（确定性） | 通过 |
| **12 个参与量单点扰动**（k 末位/1 ulp、coef/center/norm 末位、order、status、identifiable、occurrence sha、滤镜内容 sha、QE 内容 sha、滤镜键） | **0 漏**（全部换 ID） |
| ID 格式 | `photoapplied:v2:<32 hex>` |
| 修复前对照 | `std::to_string` 对两个不同 k 都给 `0.000000` |
| `response_id`：同名不同内容 ⇒ 不同 ID；缺摘要 ⇒ `@none`；无曲线 ⇒ 空 | 通过 |
| writer→reader 往返（含 §8 物理核对） | `application_id`/`response_id` 逐字保持 |

### 2.4 真实 applied E2E（身份变化对照）

输入 = 真实 Red 帧 + Gaia DR3 XP + order-2 空间增益（
`实验/normalize-precision-p5/configs/c-phot-spatial-e2e.json`）：

| | application_id | response_id |
|---|---|---|
| 修复前 | `photoapplied:87ddafa10fc32fe5fc45c056daf37a14` | `Astrodon E-series R+KAF-16803` |
| 修复后 | `photoapplied:v2:53dfb213c59081cfd9770c1a3304f465` | `Astrodon E-series R@f8ecc5f10b21+KAF-16803@03256b2a8ae6` |

同一次修复后 E2E（`evidence/run_c_phot_spatial_e2e_after_identity.log`，exit=0）：
`status=relative_only`、`scale_applied=true`、`57/57` 控制点、
**帧摘要 `reference_snr = 14.593881986033763` 与修复前逐位一致**（身份改动不影响科学
数值）；`exposure_seconds=300.0 source=input light header EXPTIME`。
修复后产品上重跑独立 α 复核：`verify_round8_real_spatial_alpha.py` **ALL PASS**
（57/57 控制点 `α_j == k_photo·m(x,y)`；m ∈ [0.868901, 1.060348]）、
`verify_round6_alpha_contract.py` **57/57 ok**。

**单元/模式回归**：`python3 -m eng.tests.unit.run_unit` ⇒ **160/160**；
`python3 eng/tests/conformance/normalize_frame_science_schema_test.py` ⇒ **18 PASS / 0 FAIL**。

未施加测光的帧（§1.5(d)）保持 `application_id=null`、`status=native_unscaled`，
**不虚构身份**。

### 2.5 兼容性（交 Codex 裁定）

* `application_id` 值格式由 `photoapplied:<32hex>` 变为 `photoapplied:v2:<32hex>`；
  `response_id` 由 `<f>+<qe>` 变为 `<f>@<sha12>+<qe>@<sha12>`。
* schema 侧无约束（`anyOf[string(minLength 1), null]`），reader 只做"施加了标度必须有
  非空 application_id"的物理核对 ⇒ 本改动不破坏读写；差异在于**下游若解析该字符串**需
  按 v2 规范（本仓内目前无解析方）。
* 影响面：同名曲线文件内容更新、同名输入 occurrence、极小 k 三类此前会**碰撞**的情形
  现在都会改变身份（这正是 Issue #5 A 的要求）。

## 3. F_syn 与 SI 光子率的单位订正（Issue #5 B）

**订正对象是验证与声明，不是生产积分核**：

* 生产：`F_syn = ∫ f_λ(λ_nm)·T·Q·λ_nm dλ_nm`，数值单位 `W·m⁻²·nm`。
* SI 光子率：`N_γ = ∫ f_λ·T·Q·λ_m/(h_SI c_SI) dλ_m = 1e-9·F_syn/(h_SI c_SI)`
  ⟺ `F_syn = 1e9·h_SI·c_SI·N_γ`；等价常数写法 `N_γ = F_syn/(1.986445857e-16 J·nm)`
  里的常数是 **h·c 取 nm 为单位**的值，与 SI `h·c = 1.986445857e-25 J·m` 相差 1e9。
* 旧测试 #5 把 λ 的 **nm 数值**直接除 SI `hc`（少 1e-9），得到的
  "`F_syn = hc·N_γ`" 是**恒等式**，不构成 SI 单位验证。
* 新测试（`code/verify_round9_fsyn_integral.cpp` 判据 5，ALL PASS，日志
  `evidence/verify_round9_fsyn_integral.log`）：(i) 直接以**米**求积的 `N_γ`
  与 `1e-9·F_syn/(h_SI c_SI)`、`F_syn/hc(J·nm)` 三种写法一致（rel 3.83e-16）；
  (ii) 往返 `F_syn == 1e9·h_SI·c_SI·N_γ`；(iii) 已知数量级判据（600 nm 光子能量
  `3.310743e-19 J`；`f_λ=1e-13 W·m⁻²·nm⁻¹` ⇒ `3.02e5 photons·s⁻¹·m⁻²·nm⁻¹`）。
* 生产数值**未变**：订正前后同一测试读到的裸 `F_syn` 完全相同
  （常量 SED `1.159380000000e-08`、幂律×窗×QE `7.0909568639e-10`、
  `flux_min` 线性 f0 `4.637520e-09`）——本部分只改实验/声明，生产积分核零改动。
* 文档同步：`ROUND9_SED_RESPONSE_CONTRACT.md` 新增 §2.0 单位关系与订正记录；
  §4 裁决点 5（星等→通量零点常数）在 sed_response 接光子数时应改为
  "先经上述 SI 换算再乘零点"。

## 4. 交接 Codex（可核对的新增事实）

1. **UAF 与实施的正确状态**：静态问题 E07 应记为**两个**缺陷（`module_adapters.cpp`
   旧 9500 行 destroy 后读 + 旧 9206 行终止路径漏 destroy），修复为"释放前取出元数据
   + 显式置空 + 补 destroy"，不是文档更正。证据：§1.5(a)/(b)。
2. **身份字段合同**：`photometry.application_id` 格式 = `photoapplied:v2:<sha256[0:32]>`，
   规范串参与量见 §2.2；`response_id` = `<filter_key>@<sha12>+<qe_key>@<sha12>`
   （内容是曲线**文件字节** sha256）。需要写进 NORMALIZE_DATA §2/§8 与 detail 合同。
3. **零点的单位链**：sed_response 接光子数时应写 `N_γ = 1e-9·F_syn/(h_SI c_SI)`，
   或明确声明所用 `hc` 的长度单位（J·nm 与 J·m 相差 1e9）。
4. **可达性事实**（可用于设计/测试文档）：EXPTIME 在本节点有两条前置依赖
   （IPV 极限星等、K=t_light/t_dark），因此"无 EXPTIME 亮帧 + 无暗场 + 显式 WCS"
   是唯一能到达后段 precision staging 的配置组合。
