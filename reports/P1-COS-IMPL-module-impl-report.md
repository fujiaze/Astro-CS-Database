# P1-COS-IMPL 交付报告 — cosmetic 模块迁移独立 DLL

- 任务: P1-COS-IMPL（队列 35，lock-P1-COS；依赖 P1-COS-TEST @ 47e00f8e 已闭环）
- 验收关键词: **DLL builds；scope scientific_change=false**
- 执行: SA-P1O-I 子代理，2026-09-09，Linux amd64 (LINUX_LIGHT)
- 依据: `tasks/MODULE_MIGRATION_TEMPLATE.md` + 先例 CAT-GAIA-IMPL (babe752d) / P1-DRZ-IMPL (2c065ace) / P1-CAL-IMPL (adf820ac)

## 1. 交付物清单

| 文件 | 内容 |
|---|---|
| `lib/cosmetic/include/astrocs/cosmetic/types.h` | 模块合同常量（module_id/version/build_id/sci/alg/api_id、plan 版本与轴、op 词表 2 op、config 词表 8 键）+ `acs_error_info_v1`/`acs_strbuf_v1` 便捷填充 |
| `lib/cosmetic/src/module_entry.cpp` | C ABI v1 九操作 adapter（query/describe/validate_config/plan/create/execute/inspect/request_cancel/destroy），纯标量迷你 JSON 解析器，inline base64 数据平面，两阶段 strbuf，事务性输出，全边界异常屏障 |
| `lib/cosmetic/src/module_exports.map` | Linux version script：`{ global: astrocs_module_query_v1; local: *; }` |
| `lib/cosmetic/src/astrocs_p1_cosmetic.def` | MSVC /DEF 白名单（Windows 面同口径） |
| `lib/cosmetic/CMakeLists.txt` | SHARED target `astrocs_p1_cosmetic`：PIC 重编译生产闭包、`AC_API=` 本地化、导出面双保险、OpenMP 自链 |
| `tests/unit/cosmetic_adapter_test.cpp` + CMake 注册 | 适配器测试 A/B/C/D/E 七组（见 §4） |
| `CMakeLists.txt`（根） | `add_subdirectory(lib/cosmetic)` + RPATH 循环补 target |
| `lib/cosmetic/module.yaml` | 交付态更新：entrypoint=`astrocs_module_query_v1`，node_operations 2 op 登记，注释面收敛至已交付事实 |

生产源零改动：`lib/calibration/src/`（cosmetic 科学实现 `cosmetic_corrector.cpp` + 全域 C API 转发层 `ac_api.cpp` 等四 TU）逐位未动 —— **scope scientific_change=false 达成**。

## 2. 关键实现决策

### 2.1 生产闭包 = calibration 4 TU 全集（CAL-IMPL 同款，实证修正）
初版按"cosmetic 域闭包 2 TU"（cosmetic_corrector + ac_api）裁剪，dlopen 实证失败：`undefined symbol: ac::generate_master` —— `ac_api.cpp` 是 calibration 全域 C API 转发层（内聚不可拆），外部引用 `ac::generate_master/_flat/calibrate/calibrate_d`（master_generator.cpp + calibrator.cpp 定义）。剔转发改生产源违反 scientific_change=false，故回退 4 TU 全编译（与 adf820ac 一致）。`hiss_format.h` 路径依赖（astro_calibration.h:172）→ 增 `lib/astro_image_io/include`。

### 2.2 导出面净化（ABI-006，`AC_API=` + version script 双保险）
- `target_compile_definitions(... AC_API=)`：经 astro_calibration.h:8 守卫本地化 legacy 12 符号，生产头零改动（P1-CAL-IMPL 引入的机制复用）。
- Linux `-Wl,--version-script` + `C/CXX_VISIBILITY_PRESET hidden`；MSVC `/DEF`。
- **实证**：`nm -D --defined-only` 输出唯一 `astrocs_module_query_v1`；`dlsym` 遗留 4 符号（ac_correct_frame/_f64/ac_set_num_threads/ac_version）全 NULL（测试 B 组断言）。

### 2.3 T-1 教训落实
SHARED 模块不链 `libastrocs_calibration.a`（STATIC 对象非 PIC），生产源独立重编译（`POSITION_INDEPENDENT_CODE ON`）。

### 2.4 OMP 教训落实（P1-CAL-INT 58d20223）
`cosmetic_adapter_test` TU 链 astrocs_calibration（天然 OMP 符号引用）且显式 `LINKER:--no-as-needed` 双保险。主构建 ctest 循环 5/5 通过（cosmetic_adapter + calibration_integration），asan 同口径 5/5。

### 2.5 线程模型 = host_executor_lease（DISP-COS-008 整改点）
execute 期经 host executor `acquire/release` 租借，注入途径 = legacy `ac_set_num_threads`（OMP ICV，不改生产源）。cpu_heavy 硬性租约：executor 缺失或 acquire 失败 → `ACS_ERR_BUDGET` detail 105，**禁单线程降级**（CAL 先例同款）。租约平衡由测试断言（acquire/release 计数 1/1；acquire 失败零 release）。

### 2.6 事务性与状态机
- 输出仅在全路径成功后写入；失败/取消路径 `out.size=0` 且不动调用方缓冲（测试 E4 以哨兵字节断言）。
- execute 完成回 CREATED 可重复；create 快照 config + execute 期全量校验覆盖双通道；double-destroy 幂等。

## 3. HEAD 偏差处置（P1-COS-TEST 登记，scientific_change=false 判定）

| 偏差 | 处置 | 依据 |
|---|---|---|
| DISP-COS-002 NaN/Inf 检测统计不过滤 | 无合同目标语义 → legacy 现状保持，负面测试按现状断言 | CAL-IMPL 风格登记 |
| DISP-COS-003 method=2 走 IDW | adapter 词表层 method 仅收 `"median"`/`"bilinear"`，词表外 → PARAM/103；科学面对任意非 0 整数走 IDW 的行为零改动 | 合同已定义合法域 = 技术收敛（允许） |
| DISP-COS-004 f64 降级位型 | `ac_correct_frame_f64` 合同即"内部降级 float32 执行"（API-COS-001 冻结）；direct-vs-plugin 对拍位型同源 | 保持 |
| DISP-COS-011 小帧自映射 | 镜像反射边界为 ALG-COS-003 冻结公式 | 无整改语义，保持 |
| DISP-COS-005 w*h 溢出 | adapter 层平面防护（111），科学面零改动 | 整改点落地 |
| DISP-COS-007/009 | execute 入口取消检查（CANCELLED 无输出）；master 键缺席 = legacy NULL 恒等通道行为保持 | 整改点落地 / 保持 |

生产源对以上全部偏差零改动。

## 4. 验证结果

### 4.1 构建与测试（主构建 + asan 双矩阵）
| 检查 | 结果 |
|---|---|
| `ninja astrocs_p1_cosmetic cosmetic_adapter_test`（主 + asan） | 通过 |
| cosmetic_adapter 全组 A–E | **PASS** |
| OMP 循环（主构建 ctest cosmetic_adapter + calibration_integration ×5） | **5/5 PASSED** |
| OMP 循环（asan ×5，含 calibration_adapter 三测） | **5/5 PASSED** |
| 主构建全量 `ctest -j4` | **107/107（1 skip 为 AVX512 环境门）** |
| asan 全量 `ctest -j4` | 102/107（见 §4.2） |
| 导出面 `nm -D` | 唯一 `astrocs_module_query_v1` |

### 4.2 asan 矩阵既有失败基线（与本任务零交集，不属本任务整改范围）
5 个失败全部为 asan 特有失败模式，失败域（gaia / mon002 / phase1_session）与本次变更文件无共享代码：
- `gaia_unshuffle`：测试故意 `malloc(SIZE_MAX)` 注入 OOM（依赖 glibc 返 NULL 语义），asan 默认 `allocation-size-too-big` 硬错。
- `mon002_alloc` / `p1_session_manifest` / `gaia_cat_adapter` / `gaia_integration`：leak 检测对既有 stub/测试自身分配报泄漏。
- 复跑补 `allocator_may_return_null=1` 后 OOM 类仍按各自断言失败，属 asan 策略配置缺口（根 CMake asan 分支 `ASAN_OPTIONS` 未含该选项），建议后续单列整改。

### 4.3 测试覆盖明细（cosmetic_adapter 七组）
- **A** 入口契约：ABI 失配 → MISMATCH；describe 错 id → MISMATCH；descriptor 与 module.yaml/types.h 三方一致（module_id/version/build_id/SCI/ALG/API、phase=1、cpu_heavy、parallel_ok、schema_ver=1）。
- **B** 导出面：dlsym 4 legacy 符号全 NULL。
- **C** validate_config 负例 detail 冻结：词表外 op=100、缺键=101、类型错=102、范围/method 词表外=103、空 config=NULL_CONFIG、未知键/重复键=CONFIG_SCHEMA、嵌套拒绝。
- **D** plan：两阶段 strbuf；`work_units=192`（16×12 数据事实）；work_unit/parallel_axis=pixel；min_workers=1；cancel_support=entry。
- **E1** f32 带 dark+bias **BITWISE** plugin == direct（memcmp 全平面 + hot/cold 计数一致）+ 租约平衡 + inspect exec_count + 可重复 execute。
- **E2** f64（奇数尺寸 9×7）BITWISE 同上（位型同源，DISP-COS-004 保持）。
- **E3** NULL-master 恒等通道 BITWISE：hot=cold=0，输出逐位等于输入。
- **E4** cancel → CANCELLED 无输出（哨兵字节未动）+ 复用再执行成功。
- **E5** budget：executor 缺失 / acquire 失败 → BUDGET+105、无输出、零泄漏 release。
- **E6** manifest 负面：空/schema_version≠1=110/尺寸不符=111/dtype 不符=111/坏 base64=112。
- **E7** 状态护栏：create 空 config → NULL_CONFIG 且 out=NULL；execute 覆盖坏 op=100 → 实例回 CREATED 可复用。

## 5. 验收对照

| 验收项 | 状态 |
|---|---|
| DLL builds（主 + asan 双矩阵） | ✅ |
| 导出面仅限已批准 C ABI（唯一 query，legacy 全 local） | ✅（nm -D + dlsym 双实证） |
| 模块测试全部通过 | ✅（cosmetic_adapter 全组 + OMP 5/5 ×2 矩阵） |
| 直接与插件对比 BITWISE 一致（f32/f64/NULL-master 三通道） | ✅ |
| plan() 真实 work_units | ✅（w×h 数据事实，不执行科学计算） |
| scientific_change=false | ✅（生产源零 diff；偏差按合同收敛或登记保持） |
