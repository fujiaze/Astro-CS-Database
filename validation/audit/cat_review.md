# ACSD v2 实现独立审核（cat 审核者）

- 审核者：cat（未参与 `src/catalogue`、`src/sources`、`src/response`、`src/photometry` 的实现）
- 审核对象范围：`src/model`、`src/numerics`、`src/operators`、`src/uncertainty`、`src/resources`、
  `src/storage`、`src/coordinates`、`src/geometry`、`src/observation`、`src/information`、`src/sky`、
  `src/coadd`、`src/workflows`、`src/projection`、`apps/cli`、`apps/contract`、`gui/`、
  `include/acsd/contracts.hpp`、`schemas/`、`docs/engineering/rebuild/`
- 审核的提交：`630e8e1d0697b50276c1c0663f21b28fc6276ca2`（`rebuild/acsd-v2`）
- 纪律：只读审核；未修改任何实现文件；未执行 `git add/commit/push`；只新建本文件与 `cat_review.json`。
  审核期间工作树被其它 Agent 并行修改，故所有结论以**上述提交的代码**为准，并用实跑证据固定。
- 未读取旧系统实现（`实验/`、`eng/`、`lib/`、`out/`、`run/`、`gaia/`、`testdata/` 的旧源码与旧测试）。

## 1. 阅读范围

`/workspace/acsd-work/brief.md`（全文）；`docs/engineering/rebuild/` 的 README、SYSTEM、OBJECTS、
API、EXECUTION、VERIFICATION（全文）；`docs/science/foundation/` 的 OBSERVATION_COORDINATES、
GAIA_RESPONSE、SPHERICAL_DRIZZLE、STATISTICS_SKY_COADD、INDEPENDENT_ORACLES（全文）；
`include/acsd/contracts.hpp`（全文 494 行）；审核范围内的 `src/*` 与 `apps/*` 实现。

## 2. 实跑命令

```text
cmake -S . -B build/audit -G Ninja -DACSD_BUILD_SCIENCE=ON      # 配置 exit 0
cmake --build build/audit                                        # 构建 exit 0（99 目标）
python3 tools/check_design.py                                    # 设计完整性 PASS
python3 validation/run_all.py                                    # 11 个必需 case，3 pass / 1 fail / 7 pending
g++ -std=c++20 -Iinclude -o /tmp/audit/u01_driver   /tmp/audit/u01_driver.cpp   \
    build/audit/libuncertainty.a build/audit/liboperators.a build/audit/libnumerics.a build/audit/libmodel.a
/tmp/audit/u01_driver                                            # 独立驱动：U01
g++ -std=c++20 -Iinclude -o /tmp/audit/shared_root /tmp/audit/shared_root.cpp   \
    build/audit/libuncertainty.a build/audit/liboperators.a build/audit/libnumerics.a build/audit/libmodel.a
/tmp/audit/shared_root                                           # 独立驱动：同源共享根与未知项
./build/audit/acsd-contract --request examples/u01.request.json --response /tmp/audit/u01.response.json
python3 /tmp/audit/cli_protocol.py build/audit/acsd              # CLI 事件与退出码
python3 /tmp/audit/product_closure.py build/audit/acsd-contract \
    .work/product-roundtrip/frame sha256:1791f9805e32b0b30f1445e49dd0472719f4cb8e88f7ea93ff0aa7fe8139a9bd
```

独立真值（U01）：在 `/tmp/audit/u01_driver.cpp` 内用自写有理数运算（`Fraction`）重算
`tᵀ R C Rᵀ t`，不 import 任何生产 helper、不读 `examples/u01.expected-model.json`。
冻结输入：`examples/u01.request.json`（R=[[.5,.5,0],[0,.25,.75]]、C=[[4,1,0],[1,9,2],[0,2,16]]、t=[1,−.5]）。
驱动只用 `#include <acsd/contracts.hpp>`，并自写最小 `Resources`/`Storage` 实现。

## 3. 逐项追踪（科学量 → producer → API → consumer → 持久化 → 内存/并行 → 独立真值）

| # | 科学量/链路 | producer → API → consumer | 持久化 / 并行 | 独立真值 | 结论 |
|---|---|---|---|---|---|
| 1 | 归一化信号 + 噪声根 | `calibrate_observation` → `NativeObservation`（`integrated_values`/`noise` 分开）→ `build_drizzle` → `publish_product` | `src/workflows/normalize.cpp`；`publish_product` 写 manifest+blocks+objects | X01（8 幅合成 ADU 端到端）**从未运行** | **未验证**（链路存在但端到端无独立真值） |
| 2 | Mosaic（sky→coadd→publish） | `build_sky_controls`/`fit_relative_sky`/`apply_sky` → `select_samples`/`solve_weights`/`coadd` → `publish_product` | `src/sky`、`src/coadd`、`src/workflows/mosaic.cpp` | S01/W01/X01 **pending** | **未验证** |
| 3 | 导出 HiPS | `build_export` → `export_signal` | `src/projection/export_projection.cpp`、`src/workflows/fits_writer.cpp` | E01 **pending**；独立 FITS reader 只覆盖单 tile | **部分验证** |
| 4 | 协方差/孔径方差 | `make_covariance` → `covariance_apply`/`quadratic_form` | `src/uncertainty` | U01 独立有理数重算 | **通过**（见 §5） |
| 5 | 同源共享根 | `NoiseModel.shared_factors` → `make_covariance` → `apply_covariance` | `src/uncertainty/api.cpp`、`covariance.cpp` | 独立驱动（同一 shared_id 两次） | **不通过**（F02） |
| 6 | 未知误差项 | `NoiseModel.unknown_terms` → `quadratic_form` | `src/uncertainty/api.cpp:148-170` | 独立驱动 case C | **通过** |
| 7 | 几何未决质量 | `compute_overlap` → `Overlaps`（`a≥0, a−A≤0, A−Ω≤0, Σa+coarse−A=0` 约束行） | `src/geometry/overlap.cpp:592` | G01（171 functional，含 work=1 / maxprecision=16/64 负例、touching_edge） | **通过** |
| 8 | 产品闭包 | `publish_product` → manifest/objects/blocks → `open_product`（新进程） | `src/workflows/product*.cpp` | 独立脚本：重开、错 manifest hash、缺块、改块、截断 manifest | **通过**（命名码有偏差，见 F06） |
| 9 | 资源与取消 | `Resources.reserve/consume_work`（原子增减+回退）→ `Context.cancellation` | `src/resources/cpu_resources.cpp:75-110`；`apps/cli/events.cpp:105-133` | R01 **pending**；CLI 控制流实测 | **部分验证**（1/4 线程一致性未实测，F09） |
| 10 | CLI/GUI 协议 | `apps/cli` JSONL 事件 + `run_summary` + 退出码 | `apps/cli/commands.cpp`、`schemas/events.schema.json` | 独立脚本按 schema 校验事件流 | **部分通过**（F01、F07、F08） |
| 11 | 一个算法一份实现 | `numerics::jacobi_svd` / `least_squares` / `cholesky_*` / `adaptive_integrate` 为唯一实现 | 全仓 grep | 源码检索 | **通过**（见 §6） |

## 4. 发现（按严重程度排序）

### F01（P1）CLI 在配置缺少必需键时崩溃，未按协议退出 2
- 位置：`apps/cli/config.cpp:86`（`document.at("inputs")`；同类调用还在 132/152/156/193/213），
  `apps/cli/*.cpp` 全文无 `try/catch`。
- 构型：`{"schema":"acsd.config/2","command":"normalize"}` → `acsd normalize --json config.json`。
- 期望：API.md「退出码：…2 配置/输入无效」「语法/解码/文件读取失败 exit2 并写同 envelope 的 result.code」，
  brief §2「不得抛异常作为协议」。
- 实际：`terminate called after throwing an instance of 'nlohmann::json_abi_v3_11_3::detail::out_of_range'
  what(): [json.exception.out_of_range.403] key 'inputs' not found`，进程 SIGABRT，
  退出码 **134**，stdout 无任何事件流。
- 最小修订：解码处改用 `contains()/find()` 的受检访问并返回解码失败；并在命令入口包一层
  `try/catch` 作为兜底，把异常映射为 exit 2 + 事件流。

### F02（P1）同一 shared 根未被合并，同源交叉项丢失
- 位置：`src/uncertainty/api.cpp`（`make_covariance` 只检查同 shared_id 根的单位冲突，不合并）、
  `src/uncertainty/covariance.cpp:apply_covariance`（`roots` 逐条施加 influence）。
- 构型：`NoiseModel.shared_factors` 两个条目，`shared_id` 相同、factor 相同（2×2 单位阵），
  influence 均为 R=[1,1]；functional t=[1]。
- 期望：同一物理源的两个 influence 必须先合并成总 influence 再作用一次 →
  (2R)(2R)ᵀ = **8**（API.md「同一 shared 根先合并总 influence」；STATISTICS_SKY_COADD.md:32
  「以独立原始随机块 ξ 为根，全部输出写成 z=f(ξ)，用 Jξ Cov(ξ) Jξᵀ 一次传播」）。
- 实际：`quadratic_form` 返回 **4**（=1+1，两条被当作独立根），实测见 `/tmp/audit/shared_root` 输出
  `case B the same shared root twice: value 4 expected 8`。
- 最小修订：`make_covariance` 在构造 C action 前按 `shared_id` 归并 `shared_factors`，
  influence 相加后只保留一条；归并后仍按 `innovations + shared_factors` 的顺序对应 influence。

### F03（P1）VERIFICATION 必需的 11 个 case 中 7 个从未运行
- 位置：`validation/run_summary.json`（本次实跑生成）、`apps/contract/driver.cpp:1412`
  的 operation 白名单（缺 `build_sky_controls`/`fit_relative_sky`/`apply_sky`/`select_samples`/
  `solve_weights`/`coadd`/`estimate_flux_information`/`evaluate_scene_variance`/
  `reconstruct_scene_variance`/`build_supported_triangulation`/`publish_product`/`normalize`/
  `mosaic`/`export_image`/`fit_psf`/`measure_science_sources`/`fit_photometry`/`apply_scale`/
  `refine_geometry`/`bind_wcs`/`bind_observation_model`/`calibrate_observation`/`regroup_observation`）。
- 构型：`python3 validation/run_all.py`。
- 期望：VERIFICATION §必需验证矩阵的 case 逐一执行并给出冻结比较。
- 实际：C01 pass、G01 pass、U01 pass；**F01 fail**（`fit_psf` 设计矩阵秩亏，
  「the design matrix is rank deficient; the singular values are 1.000000 0.000000 …」，属 `src/sources` 范围，不在本次审核域）；
  **S01、W01、V01、E01、P01、R01、X01 = pending_implementation**（observed 侧为空，
  如 S01 的 reason 为「the public sky entry points are not available yet」，
  而 `src/sky/sky.cpp` 等实现实际存在——即驱动/白名单未跟上，而非科学未实现）。
- 最小修订：扩展 `apps/contract` 的 operation 白名单（或补 `validation/native` 驱动并接入
  `validation/run_all.py` 的 DRIVERS 表），使 S01/W01/V01/E01/P01/R01/X01 可被真实调用。

### F04（P2）同 shared 根的身份冲突只比较单位乘子
- 位置：`src/uncertainty/api.cpp`（`model->shared_factors[i]->unit.multiplier != ...`）。
- 构型：两个 `shared_id` 相同、单位 scale 不同（`fixture:L` vs `other:L`）、乘子同为 1.0 的 shared 根。
- 期望：`Code::identity_conflict`。
- 实际：`make_covariance` 返回 **ok**（code 0），两种单位被混用，方差单位失去意义。
- 最小修订：比较整个 `Unit`（scale、sr/pixel/nm/linear power、multiplier）。

### F05（P2）在 Gram 矩阵上分类秩，使 rank_relative 的有效阈值变成 1e-6
- 位置：`src/information/information.cpp:350-358`（`gram` 后 `jacobi_svd(gram, rank_relative)`）。
- 构型：`estimate_flux_information` 的设计矩阵奇异值比为 1e-8（介于 1e-12 与 1e-6 之间）。
- 期望：EXECUTION「rank_relative=1e−12 只是报告分类：σ 下界>threshold 为可估」——
  阈值作用在设计矩阵的奇异值上。
- 实际：Gram 的奇异值是 σ²，阈值 1e-12 作用其上等价于对 σ 用 1e-6，比声明严格 6 个数量级，
  真实可估模态会被误判为不可估。
- 最小修订：在 `whitened`/`reduced` 上做 SVD 并分类秩，Gram 只用于投影。

### F06（P2）缺失块被报为 io_error 而非 missing_dependency
- 位置：`src/workflows/product_io.cpp`（`open_product` 的块读取路径）。
- 构型：删除已发布产品的一个 block 文件后在新进程 `open_product`。
- 期望：API.md「闭包缺失 missing_dependency」。
- 实际：返回 **io_error**（实测「a missing block is refused with the code io_error」），
  调用方无法区分「闭包不完整」与「磁盘写失败」。
- 最小修订：区分「文件不存在」与「文件不可读/校验失败」，前者用 `missing_dependency`，
  校验不符用 `hash_mismatch`（这一路径现已正确 ✓）。

### F07（P2）doctor 命令向 stdout 写多行 JSON，事件流不再是纯 JSONL
- 位置：`apps/cli/commands.cpp`（doctor 分支）。
- 期望：API.md「stdout 为 JSONL 事件」。
- 实际：doctor 在两个事件之间插入一个多行 pretty JSON 对象，GUI 的逐行 JSON 解析会失败。
- 最小修订：把诊断块写到 stderr，或作为单行事件（`event=issue`，`code=ok`）。

### F08（P2）`--version --json` 的 run_summary 缺少声明的计数字段
- 位置：`apps/cli/main.cpp`（version 分支）。
- 期望：API.md「无论退出码，run_summary 列全部输入数量、已发布/unsolved/failed/cancelled 数量与逐帧状态」。
- 实际：`--version` 的 summary 只有 `{"version":"2.0.0"}`；doctor 的 summary 字段完整 ✓。
- 最小修订：复用 `make_summary` 构造器。

### F09（P2）mosaic / export 工作流没有取消检查点
- 位置：`src/workflows/normalize.cpp:258,547` 有取消检查；
  `src/workflows/mosaic.cpp`、`src/workflows/export_workflow.cpp` 全文无 `cancellation.requested()`。
- 构型：在一次较长的 mosaic 或 export 运行中向 stdin 写
  `{"protocol":"acsd.control/2","command":"cancel","run_id":…}`。
- 期望：API.md「取消先等安全边界清理」、退出码 5。
- 实际：这些路径没有取消点，取消只在 normalize 内被观察到。
- 最小修订：在 mosaic 的逐帧循环与 export 的逐叶/逐块循环加取消检查，并映射到 exit 5。

### 观察（不计为发现）
- `acsd doctor` 报告 cfitsio 4.6.2 可用，但 FITS 读写是自写的
  （`src/workflows/fits_writer.cpp`、`apps/cli/commands.cpp`）。自写写入器已由
  `validation/apps/check_product_roundtrip.py` 的独立 reader 验证（单 tile ✓），
  但 E01（partial 支持、RA wrap、奇偶尺寸、二次 pixfrac）尚未运行。
- `src/operators`、`src/uncertainty`、几何、sky/coadd 均调用底座 `numerics::jacobi_svd`，
  未见第二份 SVD/QR/求积实现 ✓（见 §6）。
- 几何约束行 `a≥0, a−A≤0, A−Ω≤0, Σa+coarse−A=0` 在 `src/geometry/overlap.cpp:592` 显式建立 ✓，
  G01 的 touching_edge 与预算负例均 pass ✓。

## 5. 实跑通过的检查（含数字）

1. **U01 式矩阵调用（两路一致，且非回读 expected-model）**
   - 进程路径：`acsd-contract --request examples/u01.request.json` → `result.value.value = 4.328125`，
     `state=known`，数值包围 [4.328125, 4.328125] ✓。
   - 独立驱动（只 include `<acsd/contracts.hpp>`）：`quadratic_form value 4.328125`；
     `C column 0: 3.75 2`，`C column 1: 2 10.3125`（与 VERIFICATION 表一致 ✓）；
     自写有理数重算 `tᵀRCRᵀt = 19492142137212928/4503599627370496 = 4.328125` ✓。
   - 判别力：把共享 latent 拆成三个独立分量时，真值为 `20336567067344896/4503599627370496 =
     4.515625`（= 289/64）——即「拆同源假独立」的错误值与正确值相差 0.1875，该用例有判别力 ✓。
   - 驱动源码不含任何 expected-model 读取（`grep -rn expected apps/contract` 仅命中注释 ✓）。
2. **未知误差项**：`quadratic_form` 返回 `value=null`、`state=unknown`、`unknown_terms=1` ✓
   （不是零方差 ✓）；`covariance_apply` 在同一情形返回 `unavailable` ✓（源码路径 + 独立驱动 case C）。
3. **产品闭包（新进程重开）**：完整产品 `open_product` → ok（3 个对象）；
   错 manifest hash 被拒 ✓；缺块被拒 ✓；改块被拒（`hash_mismatch` ✓）；
   截断 manifest 对象表被拒（`hash_mismatch` ✓）。
4. **CLI 事件协议**：`--version --json`、`doctor --json`、缺配置、非法 JSON 配置的事件行全部通过
   `schemas/events.schema.json` 校验 ✓；seq 从 0 严格递增 ✓；run_id 唯一 ✓；恰一个 `run_finished` ✓。
5. **VERIFICATION 必需矩阵（本机实跑）**：C01 pass（69 functional）、G01 pass（171）、
   U01 pass（14）；F01 **fail**（`src/sources` 范围，非本次审核域）；S01/W01/V01/E01/P01/R01/X01
   **从未运行**。

## 6. 一个算法一份实现（检索结果）

| 算法 | 唯一实现 | 检索结论 |
|---|---|---|
| SVD / 秩分类 | `src/numerics/linalg.cpp`（`jacobi_svd`、`classify_rank`） | sky/coadd/information/coordinates 全部调用底座 ✓；无第二份实现 |
| 最小二乘 / Cholesky / 对称特征 | `src/numerics/linalg.cpp` | 审核域内无自写 QR/SVD 求解器 ✓ |
| 求积 / erf / Bessel | `src/numerics/quadrature.cpp` | 审核域内无自写求积 ✓（`src/response`、`src/sources` 另有 erf 用途，属他域） |
| HEALPix 查表 | `src/coordinates/healpix.cpp` + `chart.cpp` | `src/catalogue` 仅调用底座 ✓ |
| 线性 action / compose | `src/operators/linear_action.cpp` | 审核域内唯一 ✓ |
| 协方差作用 | `src/uncertainty/covariance.cpp` | 唯一 ✓ |
| FITS 写 / 读 | `src/workflows/fits_writer.cpp` / `apps/cli/commands.cpp` | 一写一读各一份；未使用可用的 cfitsio（观察项） |

## 7. VERIFICATION 必需案例的实跑状态

| case | 状态 | 证据 |
|---|---|---|
| C01 | **pass** | 69 functional，0 failure |
| G01 | **pass** | 171 functional（含 work=1、maxprecision=16/64 负例、touching_edge、极区） |
| U01 | **pass**（本审核独立复核） | 两路一致 4.328125，独立有理数重算一致 |
| F01 | **fail** | `fit_psf` 设计矩阵秩亏（`src/sources` 范围，非本次审核域） |
| S01 | pending_implementation | observed 侧为空；驱动白名单缺 sky 操作 |
| W01 | pending_implementation | 同上 |
| V01 | pending_implementation | 同上 |
| E01 | pending_implementation | 同上 |
| P01 | pending_implementation | 同上 |
| R01 | pending_implementation | 同上（1/4 线程、spill、取消、写盘失败均未实测） |
| X01 | pending_implementation | 同上（三命令 + 真实磁盘产品未实测） |

## 8. 剩余限制

- Windows 未实跑；真实 HST / M42 数据未验；Huber bootstrap 未验——以上均未被本次审核确认为成立。
- R01/X01 覆盖的「1/4 线程科学内容一致」「不同 H 的 spill」「取消后不发布半成品」「写盘失败」
  **没有实跑证据**，本报告不对其作任何通过声明。
- GUI（`gui/`）只做了协议层检查（事件 schema、退出码映射），**未**运行 Qt 应用本身；
  `gui/` 的独立构建与真实启动未验证。
- `src/geometry` 的真实面积只由 G01 的解析/区间真值覆盖，**独立 detector 域积分的对照**
  （G01 要求的「新 detector 域高精度区间算法 B」）在本机上仍是 val 侧证据，未由审核者重写。
- 审核期间工作树被其它 Agent 并行修改；`tools/check_design.py` 在审核开始时曾因
  `generate_contract_schema.py` 解析 `Box` 失败（另一 Agent 的在途编辑）而使整树构建失败，
  随后恢复正常——该瞬时状态未复现，不计为发现，但说明共享工作树的构建不具可重复性。
