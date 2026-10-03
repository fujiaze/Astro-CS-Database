# 审稿-P1 · ALG-platesolve-001 · 对抗审稿第 1 遍

- 片号：`ALG-platesolve-001`
- 层：`lib/algorithms/platesolve`
- 负责人：对抗审稿第 1 遍（一遍 = 对同一片材料的一次完整重读）
- 基线：仓库 `/workspace/Astro CS Database`，HEAD `850a9ede`
- 权威片清单：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:526-549`
- 结论：**需修**（无阻断级；含 1 条自洽式验收门、2 条悬空/证伪级）

> 本轮口径：负责人裁定「这个项目本身不应该有机器读，全部由 agent 亲自阅读」；「判据不可信、不必核实为通过」。
> 凡代码/注释/manifest 声称「grep 实测」「冻结」「已核验」之处，一律**当作待证伪声明**处理。
> 本片实证：**module.yaml 自称的「grep 实测」行号台账 12/12 全错**，即「实测」这一声明本身为假（见 F-3）。

---

## 1. 读完了吗

### 1.1 两种口径（必须分开看）

| 口径 | 定义 | 份数 | 行数 | 覆盖率 |
|---|---|---|---|---|
| **口径 A — 我本人逐行 `read` 原文** | 不采信任何他人转述，不采信 grep 代读 | **23/23 份全部经我本人 read 打开**（无一份完全跳过） | **4740** | **61.3%** |
| **口径 B — 我本人 + 4 个已返回子代理的并集覆盖** | 含子代理亲自 read（非 grep 代读）的行 | **23/23** | **7733** | **100%** |

**口径 A 的 4740 行逐份明细（我亲自 read 的物理行区间）：**

| 文件 | 总行 | 我读 | 我读区间 |
|---|---|---|---|
| `src/ipv_select.cpp` | 2243 | **2243** | 1–2243 全读 |
| `src/ipv_entry.cpp` | 808 | 440 | 1–440 |
| `src/ipv_robust_refine.cpp` | 1131 | 332 | 455–584、930–1131 |
| `src/ipv_sip.cpp` | 515 | 276 | 240–515 |
| `tools/diag_projection_plot.py` | 351 | 112 | 240–351 |
| `test/test_synthetic.cpp` | 293 | 110 | 100–209 |
| `test/ipv_abi_layout_lock.py` | 210 | **210** | 1–210 全读 |
| `include/ipv_log.h` | 165 | **165** | 1–165 全读 |
| `test/test_last_inlier_reset.cpp` | 152 | **152** | 1–152 全读 |
| `module.yaml` | 150 | **150** | 1–150 全读 |
| `src/ipv_kvector.cpp` | 123 | **123** | 1–123 全读 |
| `test/test_mag_iter_delivery.cpp` | 118 | **118** | 1–118 全读 |
| `test/test_triangle_budget.cpp` | 94 | **94** | 1–94 全读 |
| `cpp/ipv/build.ps1` | 90 | **90** | 1–90 全读 |
| `src/ipv_log_sink.h` | 62 | **62** | 1–62 全读 |
| `include/ipv_kvector.h` | 56 | **56** | 1–56 全读 |
| `LICENSE` | 21 | 3 | 头部 3 行（MIT / (c) 2026 fujiaze） |
| `cpp/ipv/make_clean_err.txt` | 3 | 3 | 全文 `cat -A` |
| `cpp/ipv/siril_atpmatch_b64.txt` | 1 | 1 | 全文 `od -c`/`head -c` |
| `cpp/ipv/make_clean_out.txt` | 0 | 0 | 0 字节（读 = 确认其为空） |
| `cpp/ipv/test/test_error_utf8.cpp` | 238 | **0** | ❌ 未读 |
| `IPV_PIPELINE.md` | 331 | **0** | ❌ 未读 |
| `cpp/ipv/SIRIL_COMPARISON.md` | 578 | **0** | ❌ 未读 |
| 合计 | 7733 | **4740** | **61.3%** |

### 1.2 如实列出：口径 A 下我**未逐行读完**的部分（合计 2993 行）

| 文件 | 未读区间 | 行数 | 补读来源 | 我对该段的处置 |
|---|---|---|---|---|
| `src/ipv_robust_refine.cpp` | 1–454、585–929 | 800 | 子代理 C 逐行读完 1131/1131；A 读 190–1131 | 本报告中凡落点在此段的结论，均标注「经 C 覆盖 / 我未亲读行号」 |
| `src/ipv_entry.cpp` | 441–808 | 368 | 子代理 B、C 逐行读完 808/808 | 同上 |
| `src/ipv_sip.cpp` | 1–239 | 239 | 子代理 B、C 逐行读完 515/515 | 我亲读 240–515（fit_sip 主体全部） |
| `tools/diag_projection_plot.py` | 1–239 | 239 | 子代理 A、B 逐行读完 351/351 | 我亲读 240–351（筛真信号段全部） |
| `test/test_synthetic.cpp` | 1–99、210–293 | 183 | 子代理 A、B 逐行读完 293/293 | 我亲读 100–209（验收门全部） |
| `test/test_error_utf8.cpp` | 1–238 | 238 | 子代理 A、B、C 三方逐行读完 238/238 | **我不据其下结论**；仅转述子代理「本仓最规范测试」的评价并注明非我亲读 |
| `IPV_PIPELINE.md` | 1–331 | 331 | 子代理 B 逐行读完 331/331 | 文档一致性结论均标注 B 来源 |
| `cpp/ipv/SIRIL_COMPARISON.md` | 1–578 | 578 | 子代理 B 逐行读完 578/578 | 同上 |
| `LICENSE` | 4–21 | 18 | 子代理 B 读完 | 仅就许可类型提一句，不下结论 |

**未完成事项（诚实登记）**：第 6 遍起应派专轮补 `ipv_robust_refine.cpp:1–454`（含 `rr_gauss_solve`，奇异矩阵返回语义）与 `ipv_entry.cpp:441–808`（含 5 个入口的 `validate_params_abi` 接线与 `ipv_get_last_inliers`）。我未在本轮补读，**故不在这两段下结论**。

### 1.3 我实际执行的取证动作（可复跑）

```
git -c core.quotepath=false ls-files | grep -i wcs_transform
grep -n "pragma omp" lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp
grep -n "^IPV_API " lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp
wc -l lib/algorithms/platesolve/cpp/ipv/src/ipv_{solver,select,triangle,itertrans,robust_refine,wcs,sip}.cpp
git -c core.quotepath=false grep -n "kvector_build\|kvector_query\|KVectorIndex" -- .
grep -n "acsd_p1_ipv\|P1_IPV_DIR" CMakeLists.txt
git -c core.quotepath=false grep -n "ipv_solve_from_detections_v1" lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
git -c core.quotepath=false grep -n "fit_sip" -- src include
git -c core.quotepath=false grep -n "build_wcs" -- src include test
od -c lib/algorithms/platesolve/cpp/ipv/siril_atpmatch_b64.txt
cat -A lib/algorithms/platesolve/cpp/ipv/make_clean_err.txt
```

---

## 2. 本片判定

### **需修**（不阻断，但有一条自洽式验收门必须先处理）

**最重 3 条：**

1. **🔴 自洽式验收门（生产源码，非测试）：`ipv_robust_refine.cpp:966` + `:1090`。**
   第 5 层防护写成 `if (rms_cur > initial_rms_arcsec * params.final_rms_tolerance)`，
   而 `initial_rms_arcsec`（**调用方传进来的量**）在 `:966` 被**覆写**为本模块自己算的 `baseline_rms`，
   `baseline_rms`（`:954`）= `compute_rms(compute_residuals(match_with_lowe(...)))` ——
   **与产出 `rms_cur` 的是同一组三个函数**。尺子由被测对象自己造。
   剪掉它会暴露：Y-flip、单位/尺度、坐标系约定类**系统性**缺陷对 L5 完全不可见（见 §5 反例 1）。

2. **🔴 权威 manifest 的「实测」台账是伪造的：`module.yaml:3-8`、`:10`、`:36-37`、`:29-30`。**
   12 个 C ABI 行号 **12/12 全错**（Δ+131…+156，单调增大）；而 `module.yaml:15-17` 明文宣称
   「source_symbols 为源码核对清单（P1-WCS-DOC，2026-09-07，12 个 C ABI 导出符号 **grep 实测**），禁止手抄他版」。
   同一文件 `:10` 称 7 个内核「共 13821 行」，实测 **8541**（高估 62%）。
   `:29-30` 称「未编入根 CMake 主构建（根 CMakeLists.txt 无 ipv 目标）」，被 `CMakeLists.txt:1164` **证伪**。
   ⇒ **本片最严重的不是代码 bug，是「已核验」这一声明本身为假。**

3. **🟠 NaN 通关的验收门 + 该测试在 CI 里根本不跑：`test_synthetic.cpp:150/184-187`（门）、`test_synthetic.cpp`（未注册）。**
   `if (result.rms >= 0.1) pass = false;` —— IEEE 下 `NaN >= 0.1` 为 **false** ⇒ **门通过**。
   `s_err/theta_err/tx_err/ty_err` 四道同构。任一 NaN 即 `[ALL PASS]`。
   且我实测：**`test_synthetic.cpp` 在全仓任何 CMakeLists/Makefile/yml/CTest 中被引用 0 次**。

---

## 3. 逐文件清单（读了什么 → 看到什么 → 判定）

> 行号一律仓内相对路径 + 行号。标「经 C/B 覆盖」= 该落点行不在我的口径 A 内。

### 3.1 `lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp`（2243/2243，**我全读**）

| 读了什么 | 看到什么（带 行:行） | 判定 |
|---|---|---|
| 4 条生产入口的编排段 | `987/1306/1601/1879` 四个入口把同一段 ~150 行（FOV 过滤→放宽→位次对齐→W 构建）**复制四份**，含注释已出现漂移（`1276` 有 flux 注释，`1571/1848/2182` 无） | 建议 |
| 参数合法化 | `:395-416` 对 focal/exposure/alpha_min/alpha_max **fail-closed**；紧邻 `:422-427` 对 safety/tol/max_q/zero_step/alpha_prior **静默落硬编码默认 3.0/0.1/4/3.0/0.2885**。而 `:370` 注释声称「全部参数来自 IPVSolverParams（宪章 §10.4 禁硬编码）」，`:421` 又硬编码 `13.0` | **须修**（同函数双标 + 自相矛盾） |
| 迭代失效面落盘 | `:577-589` `mag_iter_apply_to_selection` 是 11 行纯赋值；**唯一调用点 `:680` 位于三条失败 return（`:642`/`:655`/`:677`）之后** ⇒ capped / empty_sweep / 星数过少 时 `StarSelection` **一个失效字段都不写** | **须修**（详见 §4-2、§5 反例 2） |
| `capped` vs `query_failed` | `:648-656` 对 `capped` 硬拒绝；`query_failed`（`:465-475` 置位）**从不 gate**，沿用末次成功结果放行 | **须修**（`:583` 落的字段零生产消费者） |
| FOV 过滤 | `:1240-1250` / `:1540-1549` / `:1817-1826` / `:2151-2160` **四处**在 FOV 内星 <2 时静默放宽到 **1.5×FOV**，仅 `warn`，`StarSelection` **无字段记录发生过放宽**；`1.5` 为裸字面量 | **须修** |
| 选星排序 | 图像侧 `:876-884` 显式处理 NaN（排最后）；**星表侧 `:793` 无任何 NaN 处理**（`cat_mag[a] < cat_mag[b]`，NaN 与一切"相等"，stable_sort 保留 Gaia 文件遍历序）。而 `:366-367` 自己写明该序「科学有偏」 | **须修**（两侧不对称，星表侧 NaN 星可占据位次窗口） |
| 密度→n_target | `:312-316` `n_target = min(60, max(50, lround(...)))`。`query_area/img_area ≥ 6.28·k²`（k=query_radius_factor），故 `k≥0.4` 时下界 50 永不触发、上界 60 恒触发 ⇒ **`gaia_density_ratio` 在全部实际工况下对输出无影响** | **须修**（配置项可证伪为摆设） |
| Gaia 查询 | `:241` `if (ret != 0) return -1;` —— `:247-249` 的 `free` 被跳过，callee 若先分配后报错即泄漏 | 建议 |
| 面积退化 | `:296` `if (img_area_sqdeg <= 0.0) img_area_sqdeg = query_area_sqdeg;` 静默换用**另一个物理量**，无日志 | 建议 |
| 物理常量重复 | `:58` `206.265` 与 `:62` `206264.80624709636` 是同一常数的两种精度并存 | 建议 |
| 注释漂移 | `:989/1309/1605/1883` 给 `ra`/`dec` 加 `[[maybe_unused]]`，但二者在 `:1188/1227` 等被真实使用 | 建议（陈旧标注） |
| 私有线程池 | `:1223/1523/1800/2134` 四处裸 `#pragma omp parallel for schedule(static)`，`:37` 无条件 `#include <omp.h>`；`module.yaml:116` 却声明 `threading_model: host_executor_lease` | **须修**（AGENTS.md §6 明禁；manifest 自认未整改） |

### 3.2 `lib/algorithms/platesolve/cpp/ipv/src/ipv_robust_refine.cpp`（1131；**我亲读 332 行：455–584、930–1131**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **第 5 层防护 + baseline 覆写** | `:947-969` 算出 `baseline_rms` 后 **`:966 initial_rms_arcsec = baseline_rms;`**（改写调用方入参）；`:1090` 再用它当阈值。`baseline_rms` 与 `rms_cur` 出自**同一组函数**。`:943-946` 注释坦承动机是「若用 `initial_rms_arcsec` 做回退阈值，几乎所有帧都会触发回退」——**为了让门少触发而改门的尺子** | **🔴 头号发现**（生产源码里的自洽式判据） |
| 空间一致性 L3 | `:475` `if (n < params.lowe_k_neighbors + 1) return weights;` 返回**全 1.0**，**无日志**；而 `:461-463` 文件头把 L3 列为防护层。「数据不足跳过」与「检查通过全清白」返回值相同 | **须修** |
| 日志误导 | `:537-538` 格式串标 `σ=%.3f"`，实参是 `sqrt(rx[0]²+ry[0]²)` —— **第 0 个匹配点的残差模长**，与任何 σ 无关 | **须修**（数字被贴上它不支持的证据标签） |
| 日志整数除法 | `:1048` `(int)(params.diverge_match_drop * 100) / 100` —— 转型先于除法。默认 0.50 ⇒ `(int)50/100 = 0` ⇒ 日志输出「matched %zu < **0**×prev=%d」 | **须修** |
| 空集哨兵 | `:578` `if (n == 0) return 0.0;` ——「无数据」返回**完美分数** | 须修 |
| 收敛不可观测 | `converged`（`:971`）为局部变量，**从不写入 `RobustRefineResult`**；`:1081-1083` 跑满迭代只记 INFO，**仍可置 `success=true`** | **须修** |
| 健棒权重 | `:592` `if (sigma < 1e-12) return 1.0;` 在唯一调用点不可达（`:702` 已抬到 ≥1e-9）⇒ 真正行为是 MAD=0 时权重对**所有点**归零、`:758 continue` 跳过全部贡献 ⇒ **完美拟合被判成奇异矩阵**（经 A 覆盖，我未亲读该段） | 须修 |
| 结构体默认值 | `RobustRefineResult` 每字段有 `=0/=false`；`:849-851` 预置 `fallback=true, success=false`；全部失败出口不翻转标志 | ✅ **本片设计最扎实处**（经 C 覆盖） |

### 3.3 `lib/algorithms/platesolve/cpp/ipv/src/ipv_sip.cpp`（515；**我亲读 276 行：240–515**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **9 条失败出口塌缩** | `:280 result.order = 0` 起手；`:291 / :296 / :301 / :335 / :373 / :413 / :422 / :436 / :458` 全部 `return result` ⇒ 样本不足、图像尺寸非法、order<2、scale=0、正规方程奇异(A/B)、**max_coeff>100**、**RMS>10px** 全部产出 `{order=0, A=B=0}`。`order=0` 在 WCS 语义里就是「**无畸变**」——一个完全合法的替代答案。`SIPCoeffs` 无失败标志位 | **须修**（见下方裁决：非生产路径，严重度下调） |
| 消费点与可观测性 | `ipv_wcs.cpp:164-165` 传 **`nullptr`** 作 logger ⇒ 10 条 `sip_logf` 在该路径上**全哑**；`ipv_wcs.cpp:179` `if (order >= 2)` 把 ctype 切成 `RA---TAN`（合规外观的线性 WCS 头） | 须修 |
| 死变量暴露漏接护栏 | `:491-496` 算出 `max_coeff_orig` **从不使用**（`grep` 仅 492/494/495 三处，全是赋值与自比较）；`:499` 日志打的是**归一化**系数 `max_coeff`。⇒ `:436` 的 `>100` 护栏作用在归一化域，**交付到 `SIPCoeffs` 的原始域系数无任何幅度护栏** | **须修** |
| **我的裁决（推翻子代理 C 的头号定级）** | 子代理 C 把本条定为「🔴 端到端生产静默降级」。**我亲跑命令复核后否决该定级**：`fit_sip` 全仓唯一调用点是 `ipv_wcs.cpp:164`（在 `build_wcs` 内），而 `build_wcs` 全仓唯一调用点是 **`test/test_synthetic.cpp:276`**；生产走 `extract_wcs_sip`（`ipv_solver.cpp:762/1156/1454`）。⇒ `module.yaml:140` DISP-WCS-003 把 `fit_sip` 标为 **legacy 是正确的**。本条改判 **须修**（遗留路径仍编译、仍在公共头导出、其唯一测试未被 CI 注册），**不是阻断** | 须修 |
| Huber IRLS 权重算子 | `:161 irls_huber_fit` → `:410/:419` 被 `fit_sip` 调用两次 ⇒ **并非零消费者**（子代理 C 的「零消费者」预设在本条不成立；但真正的零引用者是 `NUMERIC_DIFF_STEP`，经 B 覆盖，我未亲读） | 记录 |

### 3.4 `lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp`（808；**我亲读 440 行：1–440**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| UTF-8 错误串归一 | `:319-359` `utf8_safe_copy`：`:324 cap=dst_size-1`、`:327` 双护循环、`:346` 逐字节查 NUL（**不会读过 NUL**）、`:352` 非法字节替 `'?'`、`:353` **码点边界截断**、`:357` 恒写结尾 NUL | ✅ **无溢出、无越界读**（我逐条验过；子代理 C/B 独立复核同意） |
| ABI 自描述校验 | `:202-222 validate_params_abi` 在解引用前校验 `struct_size`/`abi_version`，不匹配写 error_msg 并 return false。方向正确 | ✅ 通过 |
| **反方向的缺口** | `:417-419 ipv_get_default_params` 对调用方指针**无条件 `memset(params, 0, sizeof(IpvParams))`** —— 用**库的**结构体大小写**调用方的**缓冲。若调用方持旧 ABI 的较小 `IpvParams`，**在拿到任何校验机会之前就已越界写**。`:194-199` 的注释声称该机制「杜绝按大结构体写小缓冲」 | **须修**（ABI 自描述只单向强制） |
| 双套返回约定 | `ipv_solve*` 系：`0`=失败 / `1`=成功（`:260` `return result->success`）；`ipv_get_last_inliers`（`:472-486`）：`-1`=错误。`if (ipv_get_last_inliers(...))` 会把「错误(-1,真值)」当成功 | 须修 |
| 恒零诊断 | `:171-172` `dst->n_detected = 0; dst->n_catalog = 0;` —— **成功结果也报「检出 0 星」**；而 `:13-15` 文件头暗示仅 `success=false` 时才填占位 | **须修**（把伪造诊断值写进产品 manifest） |
| 空捕获 | `:385-387`、`:398-400`、`:411-413` 三处 `catch (...) { }` **空体无日志**；其中 `set_*_handle` 若抛 ⇒ `:397/:410` 全局句柄**不更新** ⇒ 下游 `ipv_select.cpp:1018/1022` 报「句柄未注入」= **误诊真实原因** | **须修** |
| 兜底文案掩盖 | `:178-183` `src.error` 为空时落固定串 `"ipv solve failed: no valid WCS (selection/triangle/iter_trans)"` —— 掩盖「某个失败点忘了写 error」，且串内点名三个阶段，可能指错 | 建议 |
| 代码重复 | `:230-261 do_solve_impl` 与 `:265-298 do_solve_from_memory_impl` 35 行近乎逐字重复（仅 `solve` vs `solve_from_memory`） | 建议 |
| **被证伪的注释** | `:673`「生产路径为路径 B（callback），**路径 A 仅用于 A/B 对比测试**」——**我亲跑证伪**：`orchestrator.cpp:1959` 注释「调用 `ipv_solve_from_detections_v1` 求解」、`:1964` 取符号、`:1988` 实调 ⇒ 路径 A **是生产主路径** | **须修**（注释与事实相反） |

### 3.5 `lib/algorithms/platesolve/cpp/ipv/include/ipv_log.h`（165，**我全读**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **日志通道可静默消失** | `:62-80 init()`：`sink_ = log_sink_open(path);` **无 else 分支、无返回值、无计数**。打开失败 ⇒ `enabled_` 保持 `:58` 的 `false`，**`init()` 是 void，调用方无从得知**。而 `ipv_select.cpp` 全篇的 `if (logger) logger->error(...)` 正是它的 fail-closed **上报通道** | **🔴 须修**（上报通道本身可无声消失） |
| 写盘失败被吞 | `ipv_log_sink.h:47/51` `(void)aio_atomic::append_write/append_flush(...)` 丢弃返回值；`ipv_log.h:50-51` 两函数本身返回 void ⇒ **任何写/刷失败静默丢弃** | **须修** |
| 无条件 stderr | `:120` `fprintf(stderr, ...)` 在 `if (enabled_ && sink_)` 之外 ⇒ 即便日志文件禁用仍逐条外泄；内容含用户 FITS 路径（`ipv_select.cpp:1034`）与天球指向（`:663-668`） | 建议（脱敏或纳入门控） |
| **悬空引用** | `:20-21` 称机器判据 = `eng/tools/check_ast_api.py` + `test_public_header_layering.py`。**我实测前者不存在**（全仓 `ls-files | grep check_ast_api` 仅命中 `run/` 下快照）；后者真身在 `eng/tests/backend/`（目录与文件名均与注释不符） | **须修**（「机器判据」指向不存在的门 —— 正中「判据不可信」） |
| 未初始化栈读 | `:105` `std::tm tm_buf;` 未初始化，`:106-110` `localtime_r/s` 返回值未检查，失败即格式化未初始化内存 | 建议 |
| 静默截断 | `:131/137/143/151` `char buf[1024]` 有界（无溢出），但截断无信号 | 建议 |
| 日志目录结构 | `:9-13` 描述 `logs/ipv/<frame>/` 下 4 个文件；`logs/` 目录**不存在**（经 B 覆盖） | 建议 |

### 3.6 `lib/algorithms/platesolve/cpp/ipv/src/ipv_log_sink.h`（62，**我全读**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| 分层接缝设计 | `:25-26` 公共头只声明不实现，实现放 src 侧；`:21-22` 四函数全 `inline` 不新增链接单元 | ✅ 设计正确 |
| 缺 `<new>` | `:37` `new (std::nothrow)`，但 `:25-26` 只 include 两个头，`ipv_log.h:32-38` 拉的是 `<cstddef>/<string>/<mutex>/<chrono>/<cstdio>/<ctime>/<sstream>`，**均不保证提供 `std::nothrow`** | 须修（靠传递包含侥幸成立） |
| include 面缺口 | `:26` `aio_atomic_file.h` 全仓唯一副本在 `lib/infrastructure/aio/src/`；而 `build.ps1:28` 与 `Makefile:10` 只有 `-Iinclude`（经 B 覆盖，我未亲读 Makefile）。根 `CMakeLists.txt:1173-1181` 与 `p1wcs/CMakeLists.txt:56-57` **都显式补了** ⇒ 是遗漏非设计 | **须修**（两条并列构建路径均会 `fatal error`） |

### 3.7 `lib/algorithms/platesolve/cpp/ipv/src/ipv_kvector.cpp`（123，**我全读**）+ `include/ipv_kvector.h`（56，**我全读**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| 退化索引标成功 | `:34-37` `if (n_stars < 2) { kv.built = true; return kv; }` —— **声称「已构建」而 `n_pairs=0`**，直接违反 `ipv_kvector.h:35` 的字段注释「是否已构建」。任何以 `built` 为准的调用方会把退化索引当可用 | **须修**（典型的「检查通过」机制形同虚设） |
| 失败四合一 | `:90/95/100/114` 四种不同原因（未构建 / `d_lo>d_hi` / 区间外 / 空区间）**全部返回空 vector**，无状态位 ⇒ 「没匹配」与「索引没建」不可区分 | 须修 |
| 我复核后**否决**的怀疑 | `:72-73` `front()/back()` 在空 vector 上是 UB —— **不可达**（`:34` 已挡 `n_stars<2`，故 `distances≥1`）；`:40` `(size_t)n*(n-1)/2` **无溢出**（左操作数已提升）；`:58-61` `std::sort` 仅按 `dist` 无 tiebreak，但 `distances`/`pairs` 同步填充且二分只看 `distances` ⇒ **正确** | ✅ 否决 |
| 溯源悬空 | `.h:10` / `.cpp:4` 称「提取自 `vm45_relvec.cpp`」，`vm45_relvec.cpp` 全仓不存在 | 建议 |
| 消费者分布（我亲跑） | `kvector_query` 有 3 个生产消费者（`ipv_polygon.cpp:225/316/683`）；**`kvector_build` 生产调用点 = 0**，仅 `test_kvector.cpp:281/295/325`、`test_synthetic.cpp:220` | **须修**（`IPV_PIPELINE.md:155`/`SIRIL_COMPARISON.md:129` 仍把它写成生产管线阶段 —— 经 B 覆盖） |
| 确定性声明冲突 | `module.yaml:119 determinism: fixed_reduction_order`；但 `.cpp:58` `std::sort` 对等距元素的相对序是实现定义的，而 `build.ps1`（MSYS2/MinGW）与 `Makefile`（g++）是**两套工具链** ⇒ `pairs` 顺序**跨工具链不保证逐位一致** | 须修 |

### 3.8 `lib/algorithms/platesolve/module.yaml`（150，**我全读**）— 🔴 本片最严重

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **「grep 实测」台账 12/12 全错** | 我逐个 `sed -n '<L>p'` 实测：`ipv_solve_create` 声明 `:237`→实际 **368**；`_destroy` `:249`→**380**；`_gaia` `:260`→**391**；`_detector` `:273`→**404**；`_default_params` `:286`→**417**；`_inlier_count` `:314`→**458**；`_inliers` `:328`→**472**；`_solve` `:345`→**489**；`_from_memory` `:377`→**524**；`_from_detections_v1` `:524`→**674**；`_with_callback` `:566`→**719**；`_d` `:610`→**766**。**偏差单调 +131→+156** ⇒ 某次改动后整表未刷新。`:16-17` 明文称「**grep 实测**」「禁止手抄他版」 | **🔴 阻断级文档失信** |
| 内核行数 | `:10`「共 13821 行」，我亲跑 `wc -l` 七文件 = **8541**（高估 5280 行 / 62%） | **🔴** |
| OpenMP 行锚 | `:36-37` 称 `ipv_select.cpp:810/:1123/:1412/:1756`，我实测 = **1223/1523/1800/2134**（4/4 全错）。且描述为「collapse(2) schedule(static)」，而 `ipv_triangle.cpp:302` 是裸 `parallel`、`:310` 是 `schedule(dynamic, 64)`、`:347` 是注释行 ⇒ **并行策略描述本身也错** | **🔴** |
| 「零消费者」被证伪 | `:29-30`「未编入根 CMake 主构建（根 CMakeLists.txt 无 ipv 目标）」——我亲跑 `grep -n acsd_p1_ipv CMakeLists.txt` ⇒ **`:1164 add_library(acsd_p1_ipv STATIC ...)`**，且 `:1107` `acsd_phase1_photcal` 反向 `target_link_libraries(... acsd_p1_ipv)` ⇒ 它是**真实生产依赖** | **🔴 阻断**（正是「已实测存在一例」的同形态） |
| 唯一判据缺失 | `:101` 称 `source_symbols` 收缩的判据是 `eng/ci/check_prod_wiring.py` W1 —— **`eng/ci/` 整目录不存在**（经 B 复核）⇒ 该收缩声明**不可复核** | 须修 |
| 规范源缺失 | `:15`/`:34` 引 `11_MODULE_SOURCE_TEST_STANDARD.md §4`；`:18` 引 `MODULE_MIGRATION_MATRIX.csv` —— 两者**全仓不存在**（我实测） | 须修 |
| threading_model 矛盾 | `:116 threading_model: host_executor_lease`，`:37-38` 自认「ThreadLease/omp_set_num_threads 无接线」，而实现自行开 OpenMP 4 处 | 须修 |
| DISP 归错 | `:141` DISP-WCS-004 把「AP/BP 拟合奇异半静默」归到 `ipv_wcs.cpp:477`，而该行经 B 实测是 `by[p] += bv[p]*(v/R_v);`，无该逻辑 | 须修 |
| 无参数面 | 全文无 `parameters`/`config` 节；`threading_model`/`determinism` 等键均非参数面 | 记录（与 `[P27-DEAD-PARAMS]` 披露不冲突） |
| ✅ 正确之处 | `:86-98 exports` 12 符号与 `include/ipv_api.h` **签名全部匹配**（行号除外）；`:131 legacy_paths` 两路径均存在 | ✅ |

### 3.9 `lib/algorithms/platesolve/cpp/ipv/test/test_synthetic.cpp`（293；**我亲读 110 行：100–209**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **NaN 通关 5 道验收门** | `:150 if (result.rms >= 0.1) pass = false;`、`:184 s_err >= 0.01`、`:185 theta_err >= 0.01`、`:186 tx_err >= 1.0`、`:187 ty_err >= 1.0` —— 全部写成「`x >= T` 则失败」。**IEEE 下 `NaN >= T` 为 false** ⇒ 任一量为 NaN 即 `[PASS]`。且 `:168/174/180/182` 的打印标签用**同一比较**，所以日志也显示 `[PASS]` | **🟠 须修**（恒真门，且打印与判定自洽，一致地骗人） |
| oracle 独立性（我复核后**确认成立**） | `:106-107` 用**逆变换**从 `true_transform` 生成 U；`true_transform`（s=1, θ=π/6, tx=500, ty=300）由测试自建 ⇒ **独立 oracle，成立**。子代理 A 的此判断正确 | ✅ 否决「oracle 复用实现」 |
| **未被任何构建文件注册** | 我亲跑：`test_synthetic` 在全仓 `*CMakeLists.txt`/`*Makefile`/`*.yml`/`*.yaml`/`*CTest*` 中被引用 **0 次** | **🟠 须修**（唯一的 `build_wcs` 调用方、上面 5 道门的载体，CI 从不运行） |
| 编译注释缺 include 面 | `:16-18` 的手写 g++ 命令只有 `-Iinclude`，缺 aio/src（经 B 覆盖） | 须修 |

### 3.10 `lib/algorithms/platesolve/cpp/ipv/test/test_error_utf8.cpp`（238，**我 0 行亲读**）

- 我**未亲读**，故**不据其下任何结论**。
- 转述子代理 A/B/C 三方一致的评价（标注为非我亲读）：该文件有独立手写 RFC-3629 校验器作 oracle，并含修复前 legacy 实现的**负向对照**，判据设计扎实；弱点是 2b/2c 用例缺 `success==0` 断言、2c 缺 `error_msg[0]!='\0'` 断言（空串是合法 UTF-8 ⇒ 该断言真空）。

### 3.11 `lib/algorithms/platesolve/cpp/ipv/test/test_mag_iter_delivery.cpp`（118，**我全读**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **oracle = 输入** | 被测 `mag_iter_apply_to_selection`（`ipv_select.cpp:577-589`）是 11 行纯赋值。测试手搓 `mi`（`:46-55/73-80/92-100`）后断言 `bits(sel.m_lim_final) == bits(mi.m_lim_final)`（`:59`）——**期望量就是被检量的来源**。`:106` 更直白：`bits(sel.m_lim_final) == bits(11.5)`，11.5 是测试自己 12 行前写进去的字面量 | **🟠 须修**（自洽式断言） |
| **所锁之物在生产中不存在** | 文件头 `:4-6` 声称锁「失效面必须落在交付结构 StarSelection」。但生产唯一调用点 `ipv_select.cpp:680` 位于三条失败 return **之后** ⇒ capped/empty_sweep/星数过少时 `*out_sel` **一字未写**。测试却直调 `mag_iter_apply_to_selection` 造出 `capped=true` 场景 —— **该场景在生产中不可达** | **🟠 须修**（测试绿，缺陷在） |
| 魔数无出处 | `:47` `12.9850849855`、`:55` `0.2885` 无来源说明 | 建议 |

### 3.12 `lib/algorithms/platesolve/cpp/ipv/test/test_triangle_budget.cpp`（94，**我全读**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **`f(x) == f(x)`** | `:47-48` 同一函数、字节相同实参调两次；`:51-57` 断言 `r1.success==r2.success`、`max_vote`、`top_pairs` 全等 ⇒ 等价于断言「函数是纯函数」。**`triangle_match` 恒返回 `{success=false, max_vote=0}` 时全绿** | **🟠 须修** |
| **声明的锁 ≠ 代码测的锁** | `:7` 声称「结果与**预算前**逐位一致」—— 代码里**没有任何「预算前」基准**，比的是函数与自己 | **🟠 须修** |
| 退化输入 | `:44-45` `U60`/`W60` 是**两组独立随机点**，互不对应 ⇒ `r1.success` 几乎必为 false；全文**从未断言 success==true** ⇒ 对匹配正确性零证据 | 须修 |
| ✅ 算术正确 | `:7/:63` C(60,3)=34220, ²=1.1714e9<2e9；C(66,3)=45760, ²=2.094e9>2e9 —— 我独立核算**正确** | ✅ |
| 墙钟门 | `:74/:88` `bms < 500.0` 依赖 CI 负载，会假红 | 建议 |

### 3.13 `lib/algorithms/platesolve/cpp/ipv/test/test_last_inlier_reset.cpp`（152，**我全读**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **全部失败都止步于句柄未注入** | `:74-75/86-87/104-105/121-123/141-142` 一律传 `nullptr` 作 Gaia 句柄 ⇒ 每次都死在 `ipv_select.cpp:1018` 的 `if (!gaia_handle) return -1`。文件头 `:5-7` 却声称锁「6 个入口统一重置 + **15 处失败 return** 前均置 fail_result」⇒ **15 处中至多覆盖 1 处** | **🟠 须修**（覆盖面 ≪ 声明） |
| 自选分支 | `:89-93` `if (res.success) CHECK(n>0) else CHECK(n==0)` —— 分支由被测输出自身选取；`:84` 注释自认「失败也无妨」⇒ **成功路径从未被强制** | 须修 |
| 恒等守卫 | `:108-109` `if (before > 0) CHECK(after == 0, "修复点直接验证")` —— 与 `:107` 字面同一断言；`before==0` 时整条跳过，而 `before` 由 Case 2 的条件结果决定 | 须修 |
| 声称的原因错误 | `:71`「失败调用（期望失败: **空图**）」—— 实际失败原因是句柄未注入，非空图 | 建议 |
| 注册状态（我亲跑） | 在构建文件中被引用 **1 次**（已注册）——**修正子代理 A 的说法**（A 称二者均未注册，`test_last_inlier_reset` 实为已注册） | 记录 |

### 3.14 `lib/algorithms/platesolve/cpp/ipv/test/ipv_abi_layout_lock.py`（210，**我全读**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| `--selfcheck` 的恒真成分 | `:182` 被破坏镜像**由探针数据派生**（`:66`），`:183` 再拿同一份数据去比；`:79-80` 额外把 `ABI_VERSION=999`/`STRUCT_SIZE=-1` 写死 ⇒ `:145-146/:148-150` 的常量比对**必然报错，与字段比对是否工作无关** ⇒ **无法区分「逐字段比对可用」与「比对已死但常量对不上」** | **🟠 须修**（自证伪的证明本身不严） |
| ✅ 锁本体有效 | `:84-116 compare_struct` 逐字段比 name/offset/size + `:111-115` 比 sizeof/alignof；`:139-142` 断言 `struct_size@0`/`abi_version@4` | ✅ 本体成立 |
| 断言强度 | `:113` `.get("alignof")` 返回 None 时不报错 —— 经 A 复核探针**确实输出** alignof，故当前非恒红 | ✅（A 的否决成立） |

### 3.15 `lib/algorithms/platesolve/cpp/ipv/build.ps1`（90，**我全读**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **stdout 重定向后从不读取（死锁）** | `:44-45` 同时 `RedirectStandardError=$true` 与 `RedirectStandardOutput=$true`；`:48` 只 `ReadToEnd()` **stderr**，**stdout 全程无人读**；`:49 WaitForExit()`。子进程 stdout 管道写满约 4KB 即阻塞，父进程则卡在等 stderr EOF ⇒ **永久挂起**。`:78-80` 链接步同形 | **须修** |
| 硬编码工具链 | `:9-10` 绝对路径 `C:\msys64\mingw64\bin\g++.exe` | 须修 |
| **确定性声明冲突** | `:27`/`:72` 传 `-march=native` —— 产物**不可移植、跨机器不逐位一致**，与 `module.yaml:119 determinism: fixed_reduction_order` 冲突（且 `Makefile:1-3` 自称已移除该旗标，经 B 覆盖） | **须修** |
| include 面不足 | `:28 $INCLUDES = "-Iinclude"`，缺 aio/src（§3.6） | **须修**（会导致编译失败） |
| ✅ 源文件清单 | `:19-25` 列 13 个 TU，我实测 `src/*.cpp` 恰 13 个，**13/13 全在，无多无缺** | ✅ |
| ✅ 行锚正确 | `module.yaml:28` 称构建入口 `build.ps1:27` —— 我实测 `:27` **确为** `$CXXFLAGS` 行 | ✅（不是所有行锚都错） |

### 3.16 `lib/algorithms/platesolve/tools/diag_projection_plot.py`（351；**我亲读 112 行：240–351**）

| 读了什么 | 看到什么 | 判定 |
|---|---|---|
| **筛真信号：五重筛子全部朝同一方向** | `:278 if peak_val < bkg + 1000: continue` —— 投影误差越大 ⇒ 以投影位置为中心的 17×17 窗越可能不含星峰 ⇒ **误差越大越不被统计**；`:305 if n < 3: continue` —— 外圈（畸变最重）样本稀疏的箱**整箱消失**；`:320` 只画 `bin_p25..bin_p75`（IQR），**p75→max 的尾巴永不显示**；`:329 set_ylim(0, min(20, percentile(offsets,99)*1.5))` —— **最差约 1% 被裁到画外**；`:259 mag < 11.0` 只留亮星 | **🟠 须修**（这张图的**存在理由**就是验 SIP 残差随半径变化，而它在结构上无法变红） |
| 丢弃数不可见 | `:326-327` 标题 `n=%d` 与 `:334-342` 统计只报**存活**样本，操作者看不到丢了多少 | 须修 |
| 路径解析整体失效 | `:20 _PROJECT_ROOT = dirname(__file__)` 实为 `tools/`，但 `:29/:33/:37-41/:63-66/:158` 全部按仓根拼接 ⇒ 6 个依赖无一存在，`import` 阶段即失败（经 B 逐条核实） | 须修（或整份退役） |
| 生成物落源码树 | `:52` 默认 `--output` = `tools/diag_projection.png`（经 A 覆盖） | 建议 |

### 3.17 `lib/algorithms/platesolve/IPV_PIPELINE.md`（331，**我 0 行亲读**）

- **未亲读，不下结论。** 转述子代理 B（逐行读完 331/331）：与现行代码**六处相反** —— U 坐标单位（`:91-95` 写角秒，代码 `ipv_select.cpp:1122-1123` 明写「像素，不乘 s0」）；`:326`「饱和星全选」vs 代码 `:840-847` 一律排除；`:112-114/331` n_target 上限 150/300 vs 代码 `:315-316` 恒钳 60；`:139`「mag=22 兜底」vs 代码「**删除** mag=22」；`:117` 指向**已删除**的 `estimate_mag_lim_by_density`；§五整节 flip_mode 流程 vs 已退役。文档头「版本 V4.9」落后于代码。

### 3.18 `lib/algorithms/platesolve/cpp/ipv/SIRIL_COMPARISON.md`（578，**我 0 行亲读**）

- **未亲读，不下结论。** 转述子代理 B（逐行读完 578/578）：
- `:4` 声称分析基线 `siril-1.4.3/src/registration/matching/atpmatch.c` —— **本地取证副本实为 GitHub 404**（见 §3.22）。
- `:572-576` 列出 5 个「测试结果文件」JSON —— 整个 `logs/` 目录**不存在**且被 `.gitignore:48` 忽略。
- `:502` `.trae/specs/...`、`:460` `CHANGELOG_V4.md`、`:550/553` `run_*_baseline.py`、`lib/algorithms/platesolve/python/` —— **全部不存在**。
- ⚠️ 许可风险（**非我可判，留给负责人**）：`:83-94`、`:104-114` 疑为 Siril（GPL）代码的逐行转写，却置于 MIT 许可目录；AGENTS.md §4 明禁「GPL 传染性代码复制进仓库」。

### 3.19 `LICENSE`（21，**我亲读 3 行**）

- MIT License，Copyright (c) 2026 fujiaze。与 §3.18 的 GPL 转写疑点相关，我只登记不裁决。

### 3.20 `cpp/ipv/make_clean_err.txt`（3，**我全读** `cat -A`）

- 内容是一次**失败的** `make clean`：`CreateProcess(NULL, rm -rf obj ipv_solver.dll test_kvector.exe *.o, ...) failed.` / `make (e=2): …` / `mingw32-make: *** [Makefile:65: clean] Error 2`。
- 即：**clean 从未成功过**，obj/*.o 与 ipv_solver.dll 从未被清掉。
- 该文件被 git 跟踪，且 `.gitignore` 只忽略 `build_err*.txt`/`build_out*.txt`，**未覆盖 `make_clean_*`** ⇒ 漏网。

### 3.21 `cpp/ipv/make_clean_out.txt`（0，**我确认其为空**）

- **0 字节**。零信息量的存在物，占位即误导。

### 3.22 `cpp/ipv/siril_atpmatch_b64.txt`（1，**我亲读** `od -c`/`head -c`）

- **131 字节，零个 base64 字符。** 内容 = UTF-8 BOM + `{"message":"Not Found","documentation_url":"https://docs.github.com/rest/repos/contents#get-repository-content","status":"404"}`。
- 它是一次**抓取失败的错误报文**，被命名为「base64 数据」。任务书问「是否被截断的 base64、还原后能否解出合法 PNG」—— **答：不是 base64，无需也无法解码**。
- **文件名是谎言。** `SIRIL_COMPARISON.md` 全部 Siril 侧结论建立在一份**从未取得过**的源文件上。

---

## 4. 发现清单

### 4.A 阻断（2 条）

| ID | 判定 | 位置 | 类别 |
|---|---|---|---|
| **A-1** | `module.yaml` 的「grep 实测」行号台账 **12/12 全错**（Δ+131…+156），内核行数高估 62%，OpenMP 锚点 4/4 全错且并行策略描述也错 —— 而 `:15-17` 明文自称「grep 实测、禁止手抄」 | `module.yaml:3-8,10,15-17,36-37` | 悬空引用 + 自证失信 |
| **A-2** | `module.yaml:29-30`「根 CMakeLists.txt 无 ipv 目标」被 `CMakeLists.txt:1164 add_library(acsd_p1_ipv STATIC …)` 证伪，且 `:1107` 反向链接 ⇒ 权威 manifest 误述自身集成状态 | `module.yaml:29-30` vs `CMakeLists.txt:1107,1164-1168` | 「零消费者」同形态复现 |

### 4.B 须修（14 条）

| ID | 位置 | 类别 | 一句话 |
|---|---|---|---|
| B-1 | `ipv_robust_refine.cpp:966` + `:1090` | **自洽式判据** | 验收门尺子由被测对象自己造 |
| B-2 | `test_synthetic.cpp:150,184-187` | 恒真门 | NaN 通过全部 5 道验收门 |
| B-3 | `test_synthetic.cpp`（全仓） | 死测试 | 唯一 `build_wcs` 调用方、CI 从不运行（引用 0 次） |
| B-4 | `test_mag_iter_delivery.cpp:59,106` + `ipv_select.cpp:680` | 自洽式断言 + 锁错物 | oracle=输入；所锁失效面在生产三条失败 return 后从不落盘 |
| B-5 | `test_triangle_budget.cpp:51-57` vs `:7` | 自洽式断言 | `f(x)==f(x)`；声称的「与预算前逐位一致」无基准 |
| B-6 | `ipv_select.cpp:1240-1250/1540-1549/1817-1826/2151-2160` | 静默降级 | 1.5×FOV 静默放宽 ×4，无输出字段 |
| B-7 | `ipv_select.cpp:640-656` + `:465-475` | 错误语义 | `query_failed` 落字段但零 gate；`capped` 却硬拒绝 |
| B-8 | `ipv_select.cpp:422-427` vs `:395-416` | fail-closed 伪装 | 同函数双标；`:421` 硬编码 13.0 违背 `:370` 自述 |
| B-9 | `ipv_select.cpp:793` vs `:876-884` | 数值 | 星表侧排序**无 NaN 处理**，图像侧有 —— 两侧不对称 |
| B-10 | `ipv_select.cpp:312-316` | 硬编码/死配置 | `n_target` 恒钳 [50,60] ⇒ `gaia_density_ratio` 可证伪为摆设 |
| B-11 | `ipv_log.h:62-80` + `ipv_log_sink.h:47,51` | 静默降级 | fail-closed 上报通道本身可无声消失 |
| B-12 | `ipv_entry.cpp:417-419` vs `:194-199` | ABI | 自描述只单向强制；`get_default_params` 可先越界写 |
| B-13 | `ipv_entry.cpp:673` vs `orchestrator.cpp:1959-1964,1988` | 悬空/证伪 | 注释称路径 A「仅测试」，实为生产主路径 |
| B-14 | `build.ps1:44-45,48` | 构建 | stdout 重定向后从不读取 ⇒ 管道死锁 |

### 4.B' 须修（续，8 条，归并登记）

| ID | 位置 | 一句话 |
|---|---|---|
| B-15 | `ipv_sip.cpp:291…458`（9 出口）+ `:491-496` | 9 失效面塌缩为 `order=0`（=合法「无畸变」）；交付域系数无幅度护栏（`max_coeff_orig` 死变量暴露漏接）。**注：经我复核非生产路径** |
| B-16 | `ipv_select.cpp:1223/1523/1800/2134` vs `module.yaml:116` | 算法层私开 OpenMP，违反 AGENTS.md §6「不私建线程池」 |
| B-17 | `build.ps1:27,72` + `ipv_kvector.cpp:58` vs `module.yaml:119` | `-march=native` + `std::sort` 无 tiebreak ⇒ 跨工具链不逐位一致，与 `fixed_reduction_order` 冲突 |
| B-18 | `ipv_robust_refine.cpp:537-538,1048,475,578,971` | 日志把残差模长标成 σ；`(int)(x*100)/100` 印出 0 而非 0.50；L3 静默跳过；空集返 0.0；`converged` 不落交付面 |
| B-19 | `ipv_kvector.cpp:35,90/95/100/114` | 退化索引 `built=true`；四种失败原因一律返回空 vector |
| B-20 | `ipv_abi_layout_lock.py:79-80,182-183` | selfcheck 的「非恒真」证明因常量写死而不严 |
| B-21 | `siril_atpmatch_b64.txt:1` | 404 报文冒充 base64 证据；连带 `SIRIL_COMPARISON.md` 全部 Siril 侧结论不可核 |
| B-22 | `diag_projection_plot.py:259,278,305,320,329` | 五重筛子全朝同向，系统性隐藏大残差 |

### 4.C 建议（10 条）

`ipv_select.cpp:241` 泄漏 · `:296` 面积静默换源 · `:58`/`:62` 常数双份 · `:989/1309/1605/1883` 陈旧 `[[maybe_unused]]` · `ipv_sip.cpp` 与 `ipv_wcs.cpp` 各近重复 35 行 · `ipv_log.h:105-112` 未初始化栈读 · `ipv_log.h:120` 无条件 stderr 含路径与指向 · `build.ps1:9` 硬编码绝对路径 · `test_triangle_budget.cpp:74,88` 墙钟门 · `LICENSE` 与 §3.18 GPL 疑点待裁决 · 三份残留产物移出源码树（与 `FIX_LEDGER` M1a-E-002 / M1a-I-001 两项 OPEN 对应）

### 4.D 明确否决（防误采纳）

| 怀疑 | 否决依据（我亲验或三方独立复核一致） |
|---|---|
| `utf8_safe_copy` 溢出/越界读 | `ipv_entry.cpp:324,327,346,352,353,357` 六重护栏；`error_msg` 为 `char[256]`（`ipv_api.h:59`）且容量经 `sizeof` 传入 ⇒ **无溢出、无越界** |
| 位置依赖错误码（重构即破约） | `ipv_api.h:174` 文档化 `0=失败,1=成功`；全片无 `-1/-2` 顺序码体系 ⇒ **不成立**（但契约退化为二值，见 B-7） |
| 模块直接 `exit()`/`abort()`/`throw`/`goto` | 全 10 份代码文件 grep **零命中** ⇒ 符合 AGENTS.md §6 |
| `kvector_build` 的 `front()/back()` 空 vector UB | `ipv_kvector.cpp:34` 已挡 `n_stars<2` ⇒ **不可达** |
| `(size_t)n*(n-1)/2` 整数溢出 | 左操作数已提升为 `size_t` ⇒ **无溢出** |
| `build.ps1` 列了不存在的源文件 | 13/13 全在 ⇒ **否证** |
| `ipv_log.h` 举的 `sdet_log`/`dpsf_log` 先例是编的 | 四份文件全在且被真实编译消费 ⇒ **否证** |
| 孤儿 `.pyc` | 2 个 `.pyc` 的 `.py` 源都在，且未被 git 跟踪、已被 `.gitignore:16` 忽略 ⇒ **本片无孤儿** |
| `test_synthetic.cpp` 的 oracle 复用被测实现 | `:106-107` 用**逆变换**从自建 `true_transform` 生成数据 ⇒ **独立 oracle，成立** |
| 三份残留产物被当「基线/证据」 | 全仓 grep 三者文件名仅命中 3 个文件 6 处，**全部**是审计台账把它们列为「待修残留」，无一当证据 ⇒ **不构成自愈锚** |
| `output.success` 在 `return -1` 路径残留 true | `ipv_select.cpp:997/1317/1613/1893` 用 `output = StarSelection{}` 值初始化，全对象零化 ⇒ **不成立** |
| `test_error_utf8.cpp` 是自洽断言 | A/B/C 三方独立判定其有独立手写 RFC-3629 oracle + legacy 负向对照 ⇒ **否证**（**注：此条我未亲读，仅转述**） |

---

## 5. 我主动构造的反例

### 反例 1 —— 推翻「L5 终验门能发现坐标/尺度类缺陷」· **推翻成功**

**构造**：假设 `ipv_robust_refine.cpp` 的 `apply_trans`（`:481,566` 调用）存在 **Y 轴符号错误**（该文件 `:10` 自述走 Y-up→`extract_wcs_sip` Y-flip 路径，符号错误在此是高风险面）。

**推理**：
1. `:950-951` `match_with_lowe` + `:953 compute_residuals` + `:954 compute_rms` 产出 `baseline_rms` —— 三者**都经过同一个错误的 `apply_trans`**。
2. `:966` 把 `initial_rms_arcsec` 覆写成 `baseline_rms` ⇒ **阈值携带了同一个符号错误**。
3. 主循环 `:990-1010` 用**同一个** `match_with_lowe`/`irls_fit_one_step` 产出 `rms_cur` —— 同样携带该错误。
4. `:1090 if (rms_cur > initial_rms_arcsec * 1.5)`：**被检量与期望量共享完全相同的系统误差**。
5. 系统误差在两侧相消 ⇒ 无论 Y-flip 多严重，`rms_cur ≈ baseline_rms`，**门恒绿**。

**结论**：**反例成立，L5 对系统性缺陷不可见。** 门只检验「IRLS 是否比同一起点走得更差」，而这正是任何优化器按定义会满足的。这是本片最高价值产出，且它位于**生产源码**而非测试。

**我主动给出的反面证据（防止过度解读）**：该函数另有 **4 道非自洽护栏** —— 匹配数下限（`:994/:1096 min_matched_final`）、发散检测（`:1031-1053` RMS 连升 / 匹配腰斩）、`fit_ok`（`:1012`）、候选池下限（经 C 覆盖，`:897`）。故这是「**关键验收门的 oracle 自指**」，**不是**「整函数是橡皮章」。定性：须修，定级不上升为阻断。

### 反例 2 —— 推翻「mag-iter 回归锁锁住了 P14-N-10 失效面」· **推翻成功**

**构造**：找一帧，使 Gaia 锥查询末次成功结果恰好达到 `cap_per_file`（触顶，`mi.capped=true`）。

**推理**：
1. `ipv_select.cpp:997` `output = StarSelection{}` ⇒ `output.m_lim_capped == false`。
2. `gaia_query_mag_iterative` 在 `:648` 判 `mi.capped` ⇒ `:654` 记 error ⇒ **`:655 return -1`**。
3. `mag_iter_apply_to_selection` 的**唯一**调用点在 **`:680`**，位于 `:655` **之后** ⇒ **`*out_sel` 一字未写**。
4. 于是失败最严重的帧，`output.m_lim_capped` **报 false**。
5. 回归锁 `test_mag_iter_delivery.cpp:71-87` 却断言 `CHECK(sel.m_lim_capped)` 为真且通过 —— 因为它**直调** `mag_iter_apply_to_selection` 构造 `capped=true`，而该调用方式在生产中**不可达**。

**结论**：**反例成立。测试绿，P14-N-10 缺陷在。** 这是「用同一个定义式既当被检量又当期望量」+ 「测试锁了一个生产中不存在的场景」的叠加。

### 反例 3 —— 推翻「`gaia_density_ratio` 是有效配置项」· **推翻成功**

**构造**：固定一台真实望远镜工况（f=1000mm、pixel=10µm、4000×4000、k=`gaia_query_radius_factor`=1.0）。

**手算**：
- `s0 = 206.265×10/1000 = 2.06265 "/px`；`fov_diag = √(4000²+4000²)×2.06265/3600 = 3.240°`
- `query_area = π×3.240² = 32.97 deg²`；`img_area = (4000×2.06265/3600)² = 5.2525 deg²`
- 比值 `= 6.277`
- `n_target_dbl = ratio × N × 6.277`。取 `N = 50`（`n_unsat` 常见值）：
  - `ratio = 1` ⇒ `313.8` → 钳 **60**
  - `ratio = 10` ⇒ `3138` → 钳 **60**
  - `ratio = 0.1` ⇒ `31.4` → 钳 **50**

**结论**：把 `ratio` 从 0.1 改到 10，输出恒为 60；只有 ratio < ~0.16 时才落到 50。**且 `k ≥ 0.4` 时 `n_target_dbl ≥ N ≥ 50`，下界 50 永不触发。**
⇒ **`gaia_density_ratio` 在实际工况全域内对输出无影响；`n_target` 恒为 50 或 60。** 配置项可证伪为摆设，且任何断言 `n_target >= 50` 的测试都是恒真门。

**限定（诚实）**：`ipv_solver.cpp` 另有 `img_n_target` 硬覆盖点（经 C 覆盖，我未亲读），会影响 `N` 的取值但不改变本结论的钳位结构。

### 反例 4 —— 推翻「`test_triangle_budget` 能证明预算门不改变结果」· **推翻成功**

**构造**：假设 `triangle_match` 被注入缺陷 —— `top_pairs` 排序错乱、或 `max_vote` 计算错误、或 `success` 判据写反。

**推理**：`:51-57` 全部是 `r1` 与 `r2` 的比对，而 `r1`/`r2` 是**同函数、同字节实参**的两次求值。注入缺陷对两次调用**同等生效** ⇒ 比对仍全等 ⇒ 全绿。文件头 `:7` 声称的「与预算前逐位一致」需要第三个基准（预算前的 golden），**代码中不存在**。

**结论**：反例成立。且叠加 `:44-45` 两组独立随机点、`success` 从未被断言为真 ⇒ 该文件的**证明力仅止于「函数是纯函数」与「预算未触发」**，对匹配正确性零证据。

### 反例 5 —— 推翻「`module.yaml` 的行号台账可信」· **推翻成功**

**构造**：直接对台账做逐行核对（§3.8 已列全表）。

**结论**：12/12 错，偏差**单调** +131→+156 ⇒ 整表在某次改动后未刷新；而 `:15-17` 明文称「grep 实测」。**「实测」这一声明本身为假。** 同类：`:10` 13821 vs 实测 8541；`:36-37` 4/4 锚点错且并行策略描述也错；`:29-30` 的「无 ipv 目标」被 `CMakeLists.txt:1164` 证伪。

---

## 6. 盲复算

方法：遮住他人结论，**只拿片清单 + 仓库**，独立重算三份材料的判定，再与既有判定比对。

| 对象 | 我的盲判 | 与既有判定比对 |
|---|---|---|
| `ipv_robust_refine.cpp:966/:1090` | 验收门两侧同源 ⇒ 自洽式判据 | **判一致**（三方独立命中同一条） |
| `ipv_sip.cpp` 9 出口塌缩 | 缺陷成立，**但非生产路径** | **我判偏严 → 已下调为须修**（见 §3.3 裁决） |
| `module.yaml` 行号台账 | 「实测」声明为假 | 判一致 |
| `test_synthetic.cpp` | NaN 门 + 未注册 | 判一致（子代理未报未注册一项，我补） |
| `test_last_inlier_reset.cpp` | 「15 处失败 return」声明夸大；未注册 | **我判偏严 → 修正**：实测该测试**已注册**（引用 1 次），只有 `test_synthetic` 未注册 |
| `utf8_safe_copy` 溢出 | 否决 | 判一致 |
| `fit_sip` 生产影响 | 否决「生产静默降级」 | **我判偏严于既有结论 → 已下调** |
| `select_image_stars` 排除饱和星 = 筛真信号 | 否决（有位次域对偶 + fail-closed 补偿） | 判一致（子代理 A 亦否决） |

**净结论：既有结论总体偏松**（漏了未注册测试、漏了 `compute_fov_density` 的 `void` 无失败通道、漏了 `gaia_density_ratio` 可证伪为摆设、漏了 `get_default_params` 的反方向 ABI 缺口）；**在 `fit_sip` 与 `test_last_inlier_reset` 两处既有结论偏严**，我已下调并说明理由。

---

## 7. 子代理派发记录

**派出 4 个不同职责的子代理**（A 自洽断言猎手 / B 悬空引用与退役声明 / C 静默降级与错误语义 / D 数值稳定性与硬编码）。
**3 个返回完整报告**（A、B、C）。**D（数值/硬编码/线程池）未返回报告** —— 该方向的结论本报告未采信其产出，相关发现由我本人复核补上（§3.1 私建线程池、§3.8 硬编码 13.0、§3.15 `-march=native`、§3.7 整数溢出否决、§3.5 硬编码工具链路径）。此为本轮覆盖缺口，如实登记。

### 7.1 逐条复核：采纳 / 否决

| 子代理结论 | 我的处置 | 依据 |
|---|---|---|
| C：「🔴 `fit_sip` **端到端生产**静默降级，生产调用点传 `nullptr` logger ⇒ 零可观测」 | **❌ 定级否决（降为须修）** | 我亲跑 `grep -n fit_sip/build_wcs -- src include test`：`fit_sip` 唯一调用点 `ipv_wcs.cpp:164`（在 `build_wcs` 内），`build_wcs` 唯一调用点是 **`test_synthetic.cpp:276`**；生产走 `extract_wcs_sip`（`ipv_solver.cpp:762/1156/1454`）。`module.yaml:140` 把 `fit_sip` 标 legacy **是对的**。子代理把 `ipv_wcs.cpp:164-165` 当成生产消费点，判断有误 |
| A：「`test_synthetic.cpp` 与 `test_last_inlier_reset.cpp` 均未被任何构建文件注册」 | **⚠️ 部分否决** | 我亲跑：`test_synthetic` = **0 次**；`test_last_inlier_reset` = **1 次（已注册）**。只保留前者 |
| A：「`test_synthetic.cpp` 的 oracle 独立、成立」 | ✅ 采纳 | 我亲读 `:100-118` 复核：`true_transform` 自建 + 逆变换生成，确认独立 |
| C/B：「`utf8_safe_copy` 无溢出、无越界」 | ✅ 采纳 | 我逐条复核 `:324,327,346,352,353,357` 六处护栏，确认 |
| C：「错误码**非**位置依赖，契约稳定」 | ✅ 采纳（但记其退化） | 与我 `ipv_api.h:174` 复核一致；退化性另记 B-7 |
| B：「`build.ps1` + `Makefile` 缺 aio/src include 面 ⇒ 编译失败」 | ✅ 采纳（**未亲编验证**） | 我亲验「13/13 源文件都在」「`:27` 行锚正确」，断链部分采信 B 的静态取证（唯一头副本在 `lib/infrastructure/aio/src/`，且根 CMake 已显式补该面 ⇒ 遗漏非设计）；**铁证仍需前台跑一次 make 取 stderr** |
| B：「`ipv_entry.cpp:673` 路径 A『仅测试』被证伪」 | ✅ 采纳并**独立复现** | `orchestrator.cpp:1959/1964/1988` |
| A：「`module.yaml` 『根 CMakeLists 无 ipv 目标』被证伪」 | ✅ 采纳并**独立复现** | `CMakeLists.txt:1164 add_library(acsd_p1_ipv STATIC …)`、`:1107` 反向链接 |
| A/B/C：「`module.yaml` 12 个行号全错 + 13821 行错」 | ✅ 采纳并**独立复现**（我逐个 `sed -n` 实测，偏差表见 §3.8） | — |
| B：「`siril_atpmatch_b64.txt` 实为 GitHub 404」 | ✅ 采纳并**独立复现**（`od -c` / `head -c`） | — |
| A：「`compute_rms` 只统计内点、遮蔽离群点」 | **❌ 否决** | A 自己在 F 节已自我收窄为「覆盖全部 matched 对」；真筛子在更上游的 `tol` 准入 |
| C：「`ipv_get_last_inlier_count` 返回 0 是隐藏 bug」 | **⚠️ 降级** | `ipv_api.h:252` 明文声明「0 表示无缓存或求解失败」⇒ 是**已声明的合同弱点**，非隐藏缺陷 |
| C：「`fit_sip` 未清 AP/BP 是活 bug」 | **⚠️ 否决** | 唯一调用点 `ipv_wcs.cpp:167-177` 显式全清，且 `ipv_types.h:71` 把不变量写进注释 |
| A：「`ipv_abi_layout_lock` 的 `.get("alignof")` 是恒红门」 | ❌ 否决 | 探针确实输出 `alignof` |
| A：「`ipv_log.h`/示例日志目录 `logs/ipv/<frame>/` 存在」 | ⚠️ 修正 | `logs/` 目录不存在（B 复核）；`ipv_log.h:9-13` 的目录结构是描述而非实证 |
| C：「`compute_initial_mag_cut` / `density_match_iterate` 是静默降级活 bug」 | **⚠️ 降级为死代码** | 两者仓内零调用点；且 `density_match_iterate` 的保留理由指向**已删除**的 `estimate_mag_lim_by_density` ⇒ 注释须改写（我记录，未单列阻断） |

### 7.2 我补出、子代理未覆盖的

1. `test_synthetic.cpp` 在全仓构建文件中被引用 **0 次**（我亲跑计数）—— 无人报此项。
2. `test_synthetic.cpp:150/184-187` 的 **NaN 恒真门**（A 只报了 5 道门存在，未点出 NaN 绕过；我亲读确认）。
3. `ipv_entry.cpp:417-419` `get_default_params` 的**反方向 ABI 缺口**（`memset` 用库大小写调用方缓冲）—— 无子代理报此项。
4. `ipv_entry.cpp:171-172` `n_detected/n_catalog` **恒零却写进产品 manifest** —— 无子代理报此项。
5. `ipv_select.cpp:793` 星表侧排序**缺 NaN 处理**而图像侧有 —— 无子代理报此项。
6. `gaia_density_ratio` 可证伪为摆设的**定量反例**（§5 反例 3）—— 无子代理给此推导。
7. `module.yaml:36-37` 不仅行号错，**对 `ipv_triangle` 并行策略的描述也错**（`collapse(2) schedule(static)` vs 实际 `schedule(dynamic, 64)`）—— 子代理只查到行号漂移。
8. `build.ps1:44-45,48` **stdout 重定向后从不读取**的管道死锁 —— 无子代理报此项。
9. `test_last_inlier_reset.cpp` **已注册**（修正 A 的说法）。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# ── A. 片清单核对（23 份 / 7733 行）──
sed -n '526,549p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"

# ── B-1 自洽式验收门（头号发现）──
sed -n '947,969p;1085,1095p' lib/algorithms/platesolve/cpp/ipv/src/ipv_robust_refine.cpp
#   关键：:966 initial_rms_arcsec = baseline_rms;   与   :1090 阈值用同一变量

# ── B-2 NaN 恒真门 ──
sed -n '150,155p;184,189p' lib/algorithms/platesolve/cpp/ipv/test/test_synthetic.cpp

# ── B-3 该测试未被任何构建文件注册 ──
git -c core.quotepath=false grep -l "test_synthetic" -- '*CMakeLists.txt' '*Makefile' '*.yml' '*.yaml' '*CTest*' | wc -l   # => 0

# ── B-4 mag-iter 失效面在失败路径不落盘 ──
sed -n '640,682p' lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp
#   :642/:655/:677 三条 return 全部先于 :680 的唯一映射点

# ── B-5 triangle_budget 的 f(x)==f(x) ──
sed -n '43,60p' lib/algorithms/platesolve/cpp/ipv/test/test_triangle_budget.cpp

# ── A-1/A-2 module.yaml 台账证伪 ──
for L in 237 249 260 273 286 314 328 345 377 524 566 610; do \
  printf "  claimed :%-4s actual => %s\n" "$L" "$(sed -n "${L}p" lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp)"; done
grep -n "^IPV_API " lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp
wc -l lib/algorithms/platesolve/cpp/ipv/src/ipv_{solver,select,triangle,itertrans,robust_refine,wcs,sip}.cpp   # => 8541
grep -n "pragma omp" lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp                                   # => 1223/1523/1800/2134
grep -n "acsd_p1_ipv" CMakeLists.txt                                                                          # => :1107 :1164 （证伪 module.yaml:29-30）

# ── B-13 路径 A「仅测试」被证伪 ──
grep -n "ipv_solve_from_detections_v1" lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp

# ── §3.3 我的定级裁决：fit_sip 非生产路径 ──
git -c core.quotepath=false grep -n "fit_sip" -- src include
git -c core.quotepath=false grep -n "build_wcs" -- src include test      # 唯一调用点 = test/test_synthetic.cpp
git -c core.quotepath=false grep -n "extract_wcs_sip" -- src             # 生产走此路

# ── B-21 残留产物取证 ──
od -c lib/algorithms/platesolve/cpp/ipv/siril_atpmatch_b64.txt            # 404 JSON，非 base64
cat -A lib/algorithms/platesolve/cpp/ipv/make_clean_err.txt               # 一次失败的 make clean
wc -c < lib/algorithms/platesolve/cpp/ipv/make_clean_out.txt              # => 0

# ── B-11 日志通道可静默消失 ──
sed -n '62,80p' lib/algorithms/platesolve/cpp/ipv/include/ipv_log.h
sed -n '46,52p' lib/algorithms/platesolve/cpp/ipv/src/ipv_log_sink.h       # (void) 丢弃返回值

# ── §5 反例 3：gaia_density_ratio 恒被钳位吞掉 ──
sed -n '306,317p' lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp

# ── B-14 build.ps1 管道死锁 ──
sed -n '39,50p' lib/algorithms/platesolve/cpp/ipv/build.ps1

# ── 待前台执行的铁证（本审稿人按纪律未执行）──
# make -C lib/algorithms/platesolve/cpp/ipv          # 取 stderr，验证 aio include 面断链
# powershell -File lib/algorithms/platesolve/cpp/ipv/build.ps1
```

---

## 9. 本轮未做 / 留给前台的

| 项 | 原因 |
|---|---|
| 编译、跑 ctest/pytest/任何二进制 | 纪律禁止。B-15（aio 断链）与 B-3（NaN 门）的**实跑铁证**留给前台 |
| 改任何仓内文件 | 纪律禁止（审稿人非施工人） |
| 任何 git 写 | 纪律禁止 |
| `ipv_robust_refine.cpp:1-454`、`ipv_entry.cpp:441-808` 补读 | 第 1 遍时间预算内未完成，**已如实登记**，建议第 2 遍优先 |
| 子代理 D 的报告 | 未返回；相关方向由我本人复核补上（§7） |
| `SIRIL_COMPARISON.md` 的 GPL 转写风险 | 属法律/许可判断，超出只读技术审查范围，留给负责人裁决 |
| `s->solve()` 在 `success=false` 时是否保证 `error` 非空 | `ipv_entry.cpp:178-183` 的兜底文案掩盖了这一点，需在 `ipv_solver.cpp`（片外）单独取证 ⇒ **UNRESOLVED** |
