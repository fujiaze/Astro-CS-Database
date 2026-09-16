# P1-DRZ-IMPL C++ 模块迁移先例报告（供 P1-CAL-IMPL 对齐）

作者：工程侦察员（只读工作树，drizzle 迁移尚未提交，`git status` 显示 lib/drizzle/ 与两个测试文件均为 `??`）。行号以当前工作树为准。

---

## A. lib/drizzle/src/module_entry.cpp（1319 行，C++ 编译的 C ABI adapter）

### A.0 总体包装方式

- 包装对象：`lib/healpix_db/healpix_drizzle/` 的 legacy C 接口 `hp_drizzle_api.h`（六导出，API-DRZ-001 冻结）。**注意：这层"legacy C 接口"本身是 C++ TU（hp_drizzle_api.cpp）提供的 extern "C" 门面**，内部已是 C++。
- 文件头声明技术性偏差 T-3（module_entry.cpp:4-6）：legacy 头 `hp_drizzle_api.h:14` include `<cstdint>`（C++ 头），纯 C TU 不可 include → **adapter 整个 TU 以 C++ 编译**，但全部导出面经 `extern "C"` 包裹保持 C ABI 不变（:24-26 与 :1317-1319 成对）。
- 结构分区（注释分节）：1 基础 helper(str/error/strbuf) → mini JSON 读取器（只读不分配）→ base64 编解码 → 2 config 解析校验 → 3 实例 → 4 describe/validate_config → 5 plan → 6 create/destroy/inspect/cancel → 7 execute-drizzle（帧通道）→ 8 execute-reverse → 9 execute 编排（状态机）→ 10 静态 vtable + 唯一导出。
- 科学纪律头注（:13-16）：`scientific_change=false`，adapter 只做 config/manifest 解析、PipelineFrame 组装、线程租借注入、结果/产物转发；Eriksson 剖分/面积权重/NaN-Inf 语义零触碰。
- op 词表：六导出中只有 2 个进模块面（drizzle/reverse），其余 4 个（fits_to_ahpx / reverse_capability / reverse_version）为工具通道不进 op 面，迁移不裁剪 legacy 导出（:18-20，types.h:46-47）。

### A.1 九操作实现方式

vtable 定义 :1293-1303（`static const acs_module_api_v1 g_drz_api`，8 个函数指针 + ABI 头；module_api_v1.h:124-128 另有 self_test/kernel_list/run_kernel 三个 provider 接口，drizzle 未填——"九操作"指 host 侧九步生命周期 describe→validate→plan→create→execute→inspect→cancel→destroy 等）：

1. **describe**（:504-527 `drz_describe`）：先校验 module_id 逐字节匹配（`size != strlen || memcmp != 0` → `ACS_ERR_ABI_MISMATCH`，:509-511），再填 descriptor：module/version/build/sci/alg/api id、`phase=1`（取自 module.yaml phase_scope）、`config_schema_ver`、`execution_class=0 /*cpu_heavy*/`、`parallel_ok=1`（帧内 OpenMP，线程经 host executor 租借）。
2. **validate_config**（:529-537）：直接复用 `drz_cfg_parse()`（解析+词表+有限性校验一体），配临时 cfg 后 free。config 解析器 `drz_cfg_parse`（:331-485）是全模块共享的单点：op 必填且须在词表（MISSING_FIELD/UNKNOWN_OP）、pixfrac∈[0,1] 有限、precision_mode∈{-1,0,1}、nside 必须 pow2 u32、reverse width/height ≥1 i32——每条失败都填 `err->detail_code`（DRZ_ECODE_*，types.h:88-101）。
3. **plan**（:548-611）：见 A.3。
4. **create**（:615-650）：先硬校验 `!host || !host->allocator` → `ACS_ERR_ABI_MISMATCH`（"host allocator required"，:627-631）；`drz_cfg_parse` 一次；`calloc` 出 `drz_inst`（:489-500：state、executing 互斥旗标、cfg 副本、host 指针、`volatile int cancel_req`、exec_count/last_status/last_op/last_workers/legacy_last_code 诊断字段）；**cfg 值拷贝进实例，output_path/hips_dir 的 strdup 所有权移交实例**（:645 注释），失败路径 `drz_cfg_free`。
5. **execute**（:1247-1289 编排）：状态机守卫（state 必须 CREATED 且 `executing==0`，否则 ACS_ERR_STATE，:1254-1258）→ 入口取消检查（本实例 `cancel_req` **或** `host->cancel->is_cancelled()`，:1260-1267）→ 空 manifest 拒绝 → 再 parse config → 置 `executing=1, state=EXECUTING` → 按 op 分派 `drz_execute_drizzle`/`drz_execute_reverse` → finally 语义复位 `executing=0`、EXECUTING→CREATED（:1285-1287）。
6. **inspect**（:652-670）：纯诊断 JSON（state/executing/cancel_req/exec_count/last_status/last_op/last_workers/legacy_last_code），并声明 `"cancel_support":"entry"` 与 `"threads_note":"host executor lease -> OMP ICV"`。
7. **request_cancel**（:672-678）：幂等单向置位 `cancel_req=1`（ABI-002）；DESTROYED 态 → ACS_ERR_STATE。
8. **destroy**（:680-686）：inst=NULL 空操作；`drz_cfg_free` + 置 DESTROYED + free。
9. **query 入口**（:1307-1316）：`ASTROCS_EXPORT acs_status ASTROCS_CALL astrocs_module_query_v1(host_abi, host, out_api)`；`host_abi != ACS_ABI_VERSION_V1` → ACS_ERR_ABI_MISMATCH（不降级猜测）；`out_api==NULL` → ACS_ERR_PARAM；**query 期 host 可 NULL（探针），allocator 必填校验推迟到 create**（:1311 注释）。

### A.2 legacy 包装细节（帧通道）

- 输入 manifest = **typed artifact 输入 v1 = inline base64 行数据，顶层平铺**（:690-696）：`{"op":"drizzle","width":W,"height":H,"dtype":"f32"|"f64","data_base64":"<W*H 元素>","header":{CRVAL1:"...", ...}}`。
- 适配链：`drz_rows_parse`（:756-819，width/height/dtype/data_base64 校验 + b64_decode 字节精确长度校验 `got != need` → DRZ_ECODE_DATA_DECODE）→ `drz_frame_build`（:822-848，`aio_pipeline_frame_create` + `aio_frame_add_block(frame,"data",AIO_BLOCK_FLOAT32/64,data,n,dims,2,...)`，注意 **dims[0]=H, dims[1]=W**，:832 legacy 约定）→ `drz_frame_set_header`（:715-744，冻结键词表 `kHeaderKeys` :699-710 共 34 键 + SIP 系数枚举 `{A,B,AP,BP}_{i}_{j}, i,j∈[0,5]` :712-713，逐键 `aio_frame_kv_set(frame,"header",k,v)`，失败 → DRZ_ECODE_FRAME_BUILD）→ 调 `hp_drizzle_run_hips(frame, nside, nested, pixfrac, hips_dir, output_path, &res, precision_mode)`（:920-922）。
- legacy 错误码映射 `drz_legacy_status`（:852-862）：**只转发不重解释**（DISP-DRZ 缺陷登记在案不消化）：

```cpp
// module_entry.cpp:852-862
static acs_status drz_legacy_status(int code, char* msg, size_t msg_cap,
                                    acs_error_info_v1* err,
                                    const char* legacy_err) {
    int32_t domain = ACS_ERR_DOMAIN_SCIENCE_PRECONDITION;
    if (code <= -4 && code >= -8) domain = ACS_ERR_DOMAIN_DATA;
    else if (code == -1 || code == -2 || code == -10) domain = ACS_ERR_DOMAIN_CONFIG;
    snprintf(msg, msg_cap, "drizzle: legacy_code=%d: %s", code,
             legacy_err && legacy_err[0] ? legacy_err : "(no detail)");
    efill(err, ACS_ERR_PARAM, domain, DRZ_ECODE_LEGACY_REJECT, msg);
    return ACS_ERR_PARAM;
}
```
  （domain 仅三分类；legacy 码原文进 message `"legacy_code=%d: %s"`；统一 ACS_ERR_PARAM + DRZ_ECODE_LEGACY_REJECT=120。）

- 输出 manifest：stats（n_healpix_pixels/n_source_pixels/workers=实际租借数）+ **事务 sink 产物 URI 数组 `artifacts`**——hips_dir 时探测 `signal`/`support`/`snr` 子目录存在性（`drz_artifact_exists` :865-870，`DRZ_ACCESS`/F_OK 跨平台宏 :34-40），output_path 时加 .hiss 路径（:939-986）。buffer 尺寸先按内容估算再 malloc（:940-948）。
- reverse 通道（:1008-1243）：manifest 顶层平铺 leaf 数据（`n_leaf`,`leaf_ipix_base64` u64×N,`leaf_dtype` f32|f64,`leaf_signal_base64`,可选 `leaf_support_base64` f64×N）+ WCS 数组 crval[2]/crpix[2]/cd[4] + 可选 sip_a/b/ap/bp 定长 36 数组；组装 `HpReverseDrizzleInput`（memset 0 后逐字段填，:1132-1166）调 `hp_drizzle_reverse_run`；输出 `signal_plane_base64` + `coverage_plane_base64`（b64_encode 到预分配缓冲，:1217-1225）。失败清理用 `goto rev_fail_nomem`（:1129, :1235-1242）。

### A.3 plan() work_units 计算（贴代码）

```cpp
// module_entry.cpp:563-566（真实 work_units; 元数据推导, 禁空转; :539-546 注释）
uint64_t n_pix_hp = 12ull * (uint64_t)c.nside * (uint64_t)c.nside;
uint64_t acc_bytes = n_pix_hp * 8ull * 6ull;   /* 6×f64 累加器 */
uint64_t io_out_bytes = (c.precision_mode == 1) ? n_pix_hp * 8ull * 3ull
                                                : n_pix_hp * 4ull * 3ull;
```
输出 JSON 关键字段（:582-587）：`"work_units":1, "work_units_basis":"single_frame_execute", "parallel_axis":"omp_threads", "min_workers":1, "max_workers":<cfg>, "determinism":"fixed_reduction_order"`。口径：**v1 并行轴 = 帧内 OpenMP，work_units = 单帧 execute = 1**（多帧扇出归 orchestrator 编排）；memory/io 估算全部由 config 元数据推导（accumulator = 12·nside²·8B·6；io_out = 12·nside²·dtype·3 产品；reverse 分支 = W·H·8B·2 平面，:600-602）。f64 一律 `"%.17g"` 往返精确写入（`json_append_f64` :296-298）。

### A.4 C++ exception 转稳定 status 的模式

**adapter（module_entry.cpp）内没有 try/catch**（grep 全文仅命中 cancel 语义注释）。稳定化分两层：

- 第一层在 legacy 生产源 `hp_drizzle_api.cpp` 的 C 边界（生产源已有，零改动继承）：
  - hp_drizzle_run/run_hips 唯一实现体 `run_drizzle_internal` 整体 try/catch，`std::exception` → 错误码 -11（"-1..-10 已占用"）+ memset result + setErrorMsg，`catch (...)` → "内部未知异常"；注释明言"禁止跨 extern "C" 传播"（hp_drizzle_api.cpp:1197-1212）。
  - reverse 同构 → 错误码 7（hp_drizzle_api.cpp:153-158, 379-383, 1236-1243）。
- 第二层在 adapter：非零 legacy 码 → `drz_legacy_status()`（A.2 贴码）转 `ACS_ERR_PARAM` + domain 三分类 + `DRZ_ECODE_LEGACY_REJECT` + message 原文转发。

**对 calibration 的含义**：`lib/calibration/src/ac_api.cpp` 的 extern "C" 段（:54-269）**没有 try/catch 屏障**（grep 零命中），若 ac 内部有异常路径，calibration adapter 的 C++ TU 必须自带屏障（在 adapter 侧包一层 try/catch → efill 稳定 status），或经整改点在 legacy 边界加——后者触碰科学域需走流程。见 G。

### A.5 host allocator / opaque handle 用法

- opaque handle：`acs_module_instance_v1` 在 module_api_v1.h:35 为前向声明 opaque 类型（注释"禁止猜测布局/跨 DLL 释放"）；adapter 侧 `drz_inst` 私有 struct，`drz_create` 中 `(acs_module_instance_v1*)inst` 出、各操作 cast 回（:1252）。
- host allocator：**create 期必填校验但不实际使用**——实例与缓冲全用 `calloc/malloc/free`（:636, :801, :948）；`host->allocator` 只作为 ABI 完整性探针（:627-631）。测试 stub 也提供了 alloc/free 实现（drizzle_adapter_impl.cpp:46-50, 76-81）。
- host executor（租借线程，FORBID-003 禁私建线程池）：

```cpp
// module_entry.cpp:896-916（execute-drizzle 内）
if (inst->host && inst->host->executor) {
    const acs_executor_v1* ex = inst->host->executor;
    uint32_t want = ex->max_workers;
    if (want == 0) want = ex->available_cpus;
    if (c->max_workers > 0 && c->max_workers < want) want = c->max_workers;
    if (want < 1) want = 1;
    if (ex->acquire && ex->acquire(ex->user_data, want) == 0) {
        leased = want; lease_active = 1;
        omp_prev = DRZ_OMP_GET();
        DRZ_OMP_SET((int)want);   /* omp_set_num_threads: 调用线程置 OMP ICV */
    }   /* acquire 失败 → 单线程降级执行（BUDGET 缺额非致命） */
}
```
  legacy 调用后对称 `release(leased)` + 恢复 `DRZ_OMP_SET(omp_prev)`（:923-925）。注入原理：legacy `DrizzleConfig.threads=0` → `omp_get_max_threads()`，adapter 在调用线程置 ICV 即可，不改生产源（T-2 整改点）。OpenMP 缺失时 `DRZ_OMP_SET/GET` 退化为空/1（:42-49）。`host->cancel->is_cancelled` 入口检查见 A.1-5。
- cancel 语义：legacy 无中断点 → cancel_support="entry"（仅 execute 入口检查），inspect 中如实声明（:663）。

## B. types.h（lib/drizzle/include/astrocs/drizzle/types.h，114 行）与 gaia C 版差异

先例 `lib/gaia_xpsd_client/include/astrocs/gaia/types.h`（78 行）同构：纯 POD/宏、无 STL/异常/RTTI、`extern "C"` 包裹 C11/C++17 双可编译、模块标识 5 件套 + 合同 ID + CFG_KEY 词表 + op 词表 + PLAN_VERSION + 自定义 detail_code enum 从 100 起。差异：

| 项 | gaia | drizzle |
|---|---|---|
| BUILD_ID | `"CAT-GAIA-IMPL"`（任务名） | `"p1-drz-impl-2026-09-02"`（任务名+日期，types.h:27） |
| VERSION | `"0.11.0-alpha.2"`（与 module.yaml module_version 一致） | `"1.0"`（module_entry.cpp:60 硬编码；types.h 只有 `MODULE_VERSION 1u` 数值）——**两版口径不一致，calibration 应统一** |
| op 词表 | 5 个 op 全部进模块面 | 6 legacy 导出仅 2 个进 op 面（types.h:39-47 写明取舍理由） |
| detail_code | 单段 enum 100-105（每条单行注释） | **分段注释组织**（types.h:85-101）：config(100-102) / input manifest(110-112) / execute(120) / reverse(130)，段前缀标 domain |
| 独有段 | — | `DRZ_OUT_KEY_*` 输出 manifest 键表（types.h:70-83，含 `artifacts`=事务 sink 产物 URI 数组、`workers`=实际租借数）；"legacy 错误码冻结只转发"声明段（types.h:103-108） |
| config schema | 无显式版本 | `ASTROCS_DRIZZLE_CONFIG_SCHEMA_VER 1u` + plan 输出词表版本 `ASTROCS_DRIZZLE_PLAN_VERSION 1u`（types.h:33-37，注释"词表演进必须升版本号"） |

ECODE 段全文见 types.h:88-101：`DRZ_ECODE_MISSING_FIELD=100, BAD_VALUE=101, UNKNOWN_OP=102（config/CONFIG 域）；MANIFEST_FIELD=110, DATA_DECODE=111, FRAME_BUILD=112（DATA 域）；LEGACY_REJECT=120（SCIENCE_PRECONDITION 域）；REV_SHAPE=130（DATA 域）`。通用域沿用 `ACS_DIAG_ECODE_*`(1..7)，模块专属从 100 起（ABI-002）。

## C. 导出面控制

三个文件全贴（都极短）：

```ldscript
// lib/drizzle/src/module_exports.map（6 行）
{
  global:
    astrocs_module_query_v1;
  local:
    *;
};
```

```def
; lib/drizzle/src/astrocs_p1_drizzle.def（3 行）
LIBRARY astrocs_p1_drizzle
EXPORTS
    astrocs_module_query_v1
```

CMakeLists 挂法（lib/drizzle/CMakeLists.txt:132-146，**用 `target_link_options`，不是 set_property LINK_FLAGS**）：

```cmake
# CMakeLists.txt:133-145
if(WIN32)
    # MSVC: /DEF 白名单 (legacy HP_DRIZZLE_API=dllexport 符号被 def 过滤)
    set(DRZ_DEF "${CMAKE_CURRENT_SOURCE_DIR}/src/astrocs_p1_drizzle.def")
    target_link_options(astrocs_p1_drizzle PRIVATE "/DEF:${DRZ_DEF}")
else()
    # Linux/macOS: version-script 白名单 + hidden 可见性预设
    set(DRZ_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/module_exports.map")
    target_link_options(astrocs_p1_drizzle PRIVATE
        "-Wl,--version-script=${DRZ_VERSION_SCRIPT}")
    set_target_properties(astrocs_p1_drizzle PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endif()
```

- legacy 符号降 local 机制：生产源 `hp_drizzle_api.cpp` 文件头自带 `#define HP_DRIZZLE_EXPORTS` → Windows 上 HP_DRIZZLE_API=dllexport 硬导出六符号（hp_drizzle_api.h:2-12）→ **由 /DEF 白名单过滤兜底**；Linux 上 `visibility("default")` 属性 → **version-script `local: *` 兜底**，另加三层 hidden 可见性预设。CMake 注释明令**不得在 target 上定义 `HP_DRIZZLE_EXPORTS`**（:107-109），生产源零改动。
- 双路径小结：Linux/macOS = version-script + visibility hidden 预设；Windows = /DEF。无 `WINDOWS_EXPORT_ALL_SYMBOLS`（对照根 CMakeLists astrocs_cpu_baseline 反而开了它——那是 legacy ABI 预览，非模块面）。

### C.2 lib/drizzle/CMakeLists.txt 其余要点（对 SHARED 化关键）

- **不链 STATIC .a 而是全闭包源重编译**（T-1 事实，:4-8, :35-38）：现有 STATIC 库对象非 PIC（TLS local-exec 重定位 R_X86_64_TPOFF32 链 .so 失败实证）→ `DRZ_PROD_SOURCES`（9 个 healpix_drizzle 生产源 :20-30，与根 astrocs_drizzle :368-377 逐文件一致）+ `DRZ_AIO_SOURCES`（aio/hiss/healpix/sha256 等 18 文件 :39-59）+ cfitsio 源（根变量 `ASTROCS_CFITSIO_SOURCES` 相对路径需 foreach 前缀变换到 `${CMAKE_SOURCE_DIR}/`，:66-70）全部进 SHARED，`POSITION_INDEPENDENT_CODE ON`（:73）。
- `PREFIX ""` + `OUTPUT_NAME "astrocs_p1_drizzle"`（:74-75，产物名与 module.yaml dll_name 一致、无 lib 前缀）；`C_STANDARD 11 / CXX_STANDARD 17`（:76-78）。
- include 路径写死（因本子目录注册于根 :184，早于 astrocs_aio/astrocs_drizzle 等 target 定义，无法 target 名继承，:14-16 注释）；第三方（cfitsio、nlohmann、可选 nanoflann）走 `SYSTEM PRIVATE` 隔离（:91-99）。
- 编译宏：`AIO_ENABLE_FITS AIO_ENABLE_HEALPIX ASTROCS_ABI_SHARED=1 ASTROCS_ABI_EXPORTS=1`（:102-110）。
- 链接只剩系统库：Threads::Threads + 可选 OpenMP::OpenMP_CXX（`$<$<COMPILE_LANGUAGE:CXX>:-fopenmp>` 仅 CXX 语言）+ 可选 ZLIB + UNIX `m`（:117-130）。

### C.3 根 CMakeLists.txt 注册段

```cmake
# 根 CMakeLists.txt:179-184（gaia 之后、astrocs_cpu_baseline 之前）
# astrocs_p1_drizzle: drizzle 迁移模块 DLL (P1-DRZ-IMPL; 矩阵行
# astrocs.p1.drizzle / astrocs_p1_drizzle.dll)。九操作 C ABI v1 adapter
# 包装 legacy healpix_drizzle 生产源 (API-DRZ-001 冻结, scientific_change=
# false 零改动重编译); 唯一导出 astrocs_module_query_v1, legacy 六符号经
# version-script 降 local (模块 CMakeLists)。
add_subdirectory(lib/drizzle)
```

RPATH foreach（根 :208-214，注意 drizzle 已被列入）：

```cmake
# 根 CMakeLists.txt:208-214
foreach(tgt astrocs_runtime astrocs_io astrocs_noop astrocs_cpu_baseline astrocs_p1_drizzle)
  if(UNIX AND NOT APPLE)
    set_target_properties(${tgt} PROPERTIES
      INSTALL_RPATH "$ORIGIN" BUILD_RPATH_USE_ORIGIN YES)
  endif()
endforeach()
```

## D. module.yaml 更新内容（lib/drizzle/module.yaml，62 行）

**drizzle 版仍是迁移前状态，未更新**（与 gaia 已更新版对照）：

- drizzle 当前值（P1-DRZ-DOC 冻结 2026-09-07）：`module_status: CONTRACT_READY`、**`entrypoint: MISSING`**、**`node_operations: []`**（:33-35）；`dll_name: astrocs_p1_drizzle.dll`、`threading_model: host_executor_lease`、`resource_class: cpu_heavy`、`cpu_providers: [baseline]`、`determinism: fixed_reduction_order`（:59-62）；头注 :21-26 记录 entrypoint=MISSING 的证据链（registry descriptor lib/core/src/module_adapters.cpp:508-525 未接节点、session 无 drizzle stage、生产调用走遗留 orchestrator DLL 通道）。
- gaia 对照（lib/gaia_xpsd_client/module.yaml，CAT-GAIA-IMPL 后已改）：`entrypoint: astrocs_module_query_v1`（:20）；`node_operations:` 逐 op 登记 = types.h op 词表全量（:21-26，"每 (module_id, operation) 绑定同一唯一 entry"，头注 :7-12 含 nm -D 实证与三方一致复验说法）。
- **calibration 对齐点**：P1-CAL-IMPL 落地后应照 gaia 模式把 entrypoint/node_operations 补上（drizzle 侧这两行尚欠，属 P1-DRZ-IMPL 未完事项/或 DOC 更新任务）。

## E. tests/unit adapter 测试

文件（均未提交，`??`）：
- `tests/unit/drizzle_adapter_test.cpp`（173 行，主测试；文件头注释仍写 ".c"——笔误遗留）
- `tests/unit/drizzle_adapter_impl.cpp`（347 行，被 `#include` 进主测试的实现文件，:22 `#include "drizzle_adapter_impl.cpp"`——单 TU 直径，非链接分离）

CMake 注册（tests/unit/CMakeLists.txt:759-781）：

```cmake
# tests/unit/CMakeLists.txt:768-781
add_executable(drizzle_adapter_test drizzle_adapter_test.cpp)
target_include_directories(drizzle_adapter_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/healpix_db/healpix_drizzle
  ...（略）
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/drizzle/include)
target_compile_definitions(drizzle_adapter_test PRIVATE ...)
target_link_libraries(drizzle_adapter_test PRIVATE
  astrocs_drizzle ${CMAKE_DL_LIBS} ${ASTROCS_OMP_TARGET})
add_dependencies(drizzle_adapter_test astrocs_p1_drizzle)
add_test(NAME drizzle_adapter COMMAND drizzle_adapter_test)
set_tests_properties(drizzle_adapter PROPERTIES
  ENVIRONMENT "ASTROCS_DRIZZLE_DLL_PATH=$<TARGET_FILE:astrocs_p1_drizzle>")
```

**加载方式：dlopen/LoadLibraryA 双路径，非直接链接 DLL**。测试可执行静态链 `astrocs_drizzle` STATIC 库获得 direct 路径 legacy 符号，plugin 路径经环境变量 `ASTROCS_DRIZZLE_DLL_PATH`（CMake test properties 注入 `$<TARGET_FILE:...>`）dlopen DLL，`dlsym("astrocs_module_query_v1")` 取入口（drizzle_adapter_test.cpp:50-62）；`add_dependencies` 保证 DLL 先构建。

覆盖面：
- **A 负面 ABI**：`entry(999,...) → ACS_ERR_ABI_MISMATCH`、`out_api=NULL → ACS_ERR_PARAM`（:67-70）。
- **B 导出面负面**：`dlsym(h,"hp_drizzle_run"/"hp_drizzle_run_hips"/"hp_drizzle_reverse_run")` 必须 == NULL（:76-83，Windows 走 GetProcAddress）。
- **C 生命周期/负面 config**：describe 错 module_id → MISMATCH（:91）；validate unknown op → PARAM/UNKNOWN_OP、nside 非 2 幂 → PARAM/BAD_VALUE、pixfrac>1 → PARAM/BAD_VALUE（:106-116，逐条断言 `err.detail_code`）；plan 断言 `"work_units":1`、`"n_healpix_pixels":3145728`（12·512²，:126-129）；inspect 串断言；request_cancel 幂等两次 OK，随后 execute → ACS_ERR_CANCELLED（:153-157）。
- **D direct-vs-plugin**：见 F。
- **E 租借**：host stub `drz_acquire`/`drz_release` 计数断言 `acquire_calls>0 && release_calls>0`（:159-160）；stub 带 `fail_acquire` 开关（impl :43, 52-58）留降级路径测试位。
- host stub 结构（impl :38-83）：栈上 `drz_test_host{api,executor,allocator,计数}`，逐 head 填 struct_size/abi_version，`available_cpus=4, max_workers=4`——calibration 测试可直接仿制。

## F. direct-vs-plugin 比对

- **口径 = BITWISE 逐位一致，非容差**（两处）：
  1. drizzle op：同 fixture（64×64 f32 内存合成高斯斑块，WCS 键值齐备）→ direct `hp_drizzle_run(frame,512,1,1.0,NULL,&res,0)`（output_path=NULL 纯统计，impl :166-170）vs plugin execute（output_path 指向临时 hips_dir，mkdtemp）→ 断言输出 JSON 中 `"n_healpix_pixels":<direct 值>` 与 `"n_source_pixels":<direct 值>` 逐位相同（impl :241-256）。**统计面对比绕开 .hiss 文件内 history elapsed=%.3fs 差异**（test 头注 :14-16——这是避坑关键：不要 memcmp 文件）。
  2. reverse op：direct `hp_drizzle_reverse_run(&rin, plane_direct, plane_plugin, &rres)` 输出平面 vs plugin 输出 `signal_plane_base64` 解码后 `memcmp(plane_direct, plane_plugin, plane_bytes) == 0`（impl :302-346）。
- 组织方式：direct 与 plugin 共用同一 manifest 组装 helper（`drz_build_drizzle_manifest`/`drz_build_reverse_manifest`，impl :120-162）与同一 host stub；plugin 侧 `api->execute(inst, manifest, cfg, &ob, &err)` 一次调用；测试自带 base64 编解码两份独立实现（impl :99-118 与 :324-344，与 adapter 内 :225-284 同算法）。
- 小 nside 反向允许（NSIDE=64，impl :141 注释"LEGACY 小 nside 反向允许"）；256 leaf 合成子集而非全 12·64²。

## G. calibration 可复用模式与要避开的坑（作者判断）

**直接复用**：
1. 文件骨架与分区（module_entry.cpp 十节结构）+ `extern "C"` 包裹整个 TU 的 T-3 模式——calibration 若被 legacy 头拖入 C++ 头（`<cstdint>` 等）同理处理；types.h 若能保持纯 C 则照 gaia 版。
2. helper 全套照抄：`efill`（error 填充）、`strbuf_write/_cstr`（冻结截断语义：size=所需、cap=0 只问尺寸、不足 → PARAM+BUFFER_TOO_SMALL）、mini JSON 只读解析器、base64 编解码、`json_append_f64` "%.17g"。
3. config 解析单点共享：`drz_cfg_parse` 一份代码供 validate_config/plan/create/execute 四处复用，每条失败带 DRZ_ECODE_*；calibration 应同样先写一份 `cal_cfg_parse` + ECODE 段（config/manifest/legacy 三段式编号 100/110/120）。
4. SHARED 化方案：**全闭包源重编译而非链非 PIC STATIC .a**（T-1：TLS local-exec 重定位链 .so 失败实证）——calibration 生产源若同样在根 STATIC 库（astrocs_calibration 在根 :511 附近被链），照抄 DRZ_PROD_SOURCES/DRZ_AIO_SOURCES/cfitsio foreach 前缀变换三段；`POSITION_INDEPENDENT_CODE ON` + `PREFIX ""` + 宏/编译选项与根 STATIC 逐项对齐 + 第三方 SYSTEM 隔离。
5. 导出面：module_exports.map（global 仅 query，local:*）+ 3 行 .def + `target_link_options`（WIN32 /DEF，else version-script）+ 三 hidden 可见性预设；根 CMakeLists 注册注释段 + RPATH foreach 追加 target 名。
6. 测试三件套：主测试(.cpp) + impl include 单 TU + CMake `add_dependencies` + `ASTROCS_*_DLL_PATH=$<TARGET_FILE:...>` 注入 dlopen；负面 ABI / 导出面 dlsym==NULL / detail_code 逐条 / plan work_units 断言 / cancel 幂等 / 租借计数 stub 全部照抄。
7. direct-vs-plugin BITWISE 统计面/平面 memcmp 口径与"绕开文件 history 时间戳"技巧；fixture 内存合成 + manifest base64 组装 helper。
8. 线程租借注入模式（host executor acquire → omp_set_num_threads → release → 恢复 prev ICV；acquire 失败单线程降级非致命）与 cancel_support="entry" 如实声明；inspect 输出 legacy_last_code 便于诊断。
9. plan work_units 语义（单帧 execute=1，元数据推导 memory/io 估算，basis/parallel_axis/determinism 字段）。

**要避开的坑**：
1. **异常屏障缺口**：drizzle 例外屏障在生产源 hp_drizzle_api.cpp 已有（-11/码 7），adapter 因此无需 try/catch；但 **calibration 的 ac_api.cpp extern "C" 段（:54-269）没有任何 try/catch**——calibration adapter 必须**自己加** C++ exception → 稳定 acs_status 的屏障（catch(...) → efill PARAM/INTERNAL + 稳定 ECODE），否则异常跨 extern "C" 逃逸 = UB。改 legacy 边界属科学域改动，需走流程，优先在 adapter 侧做。
2. **version 双口径**：types.h `"1.0"`（entry :60）vs module.yaml `module_version: 0.11.0-alpha.2`；gaia 用同一字符串三方一致。calibration 从一开始就统一成 module.yaml 的 module_version。
3. **module.yaml 欠账**：drizzle 至今 entrypoint: MISSING / node_operations: []（迁移代码已在飞但 yaml 未跟）；calibration 落地时把 entrypoint=astrocs_module_query_v1 与 op 词表登记进 yaml（照 gaia :20-26），别留同类欠账。
4. **dims 顺序约定**：`aio_frame_add_block` dims[0]=H、dims[1]=W（module_entry.cpp:832 明注 legacy 约定）——calibration 帧通道同样要核对 aio/legacy 的 H/W 惯例，别想当然。
5. **manifest 顶层平铺 + base64 行数据**是 v1 typed artifact 输入的既定口径（types.h:41-43），calibration 若输入也是帧数据应沿用（host 侧已有消费先例），不要发明嵌套结构。
6. **导出宏陷阱**：不要在 target 上定义 legacy 的 `HP_DRIZZLE_EXPORTS` 类宏（生产源自带；:107-109）；legacy 符号净化完全靠链接器白名单兜底，测试必须包含 dlsym==NULL 断言。
7. **事务 sink 输出**：输出 manifest 的 `artifacts` URI 数组 + 存在性探测（hips_dir/signal/support/snr 模式）——calibration 输出 master/dark 等产物照此报 URI，别只回统计。
8. **不要 memcmp 产物文件**做比对（内嵌 elapsed 时间戳），比统计面或纯数据平面。
9. strbuf 输出 buffer 先按内容估算再 malloc 的模式（:940-948）在 reverse 输出按 `b64_encoded_len(npix*out_b)*2` 精确估——calibration 输出大平面时照此估，别固定栈缓冲。
10. 测试文件名/头注释笔误（.cpp 文件头写 ".c"）无害但别复制；`#include "*.cpp"` 单 TU 直径是刻意为之（direct 路径与生产源同编译单元语义），不要"修"成分离链接。

**calibration 特有差异预判**（侦察所见，供 parent 参考）：lib/calibration 结构 = src/（ac_api.cpp 已是 extern "C" 门面 + calibrator/dark_optimizer/master_generator 等内部 C++）+ cpp/cosmetic_corrector + include/astro_calibration.h（extern "C"）；即 calibration legacy 对外面已是 C ABI、无 C++ 类型泄漏，adapter 直接调 C 门面即可（比 drizzle 还干净），风险集中在异常屏障（坑 1）与帧通道输入装配。
