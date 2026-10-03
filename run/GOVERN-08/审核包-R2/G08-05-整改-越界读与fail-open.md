# G08-05 · 越界读 + 静默改判 与同目录其余 fail-open —— 整改与自证

| 项 | 值 |
|---|---|
| 单号 | GOVERN-08 / G08-05（执行子单：改生产源码） |
| 核心目标 | `lib/algorithms/coverage/src/rejection.cpp` 越界读 + 静默把样本写成「已接受」，**单一事实源**整改；同目录其余 fail-open 一并处置 |
| 权威依据 | `docs/science/REJECTION.md` §3(:30-33) / §4(:52) / §5(:146)；`docs/science/PHASE2_UPM.md`（σ_eff、节点间距、k_corr 域）；`docs/science/DATA_SEMANTICS.md`(:1748)；`lib/algorithms/coverage/include/astro/phase2/{rejection,upm,sky_plane}.h` 合同 |
| 改动文件 | `lib/algorithms/coverage/src/rejection.cpp`、`upm.cpp`、`sky_plane.cpp`（共 3 个文件，+139/−9） |
| 本轮**未执行** | 项目编译、ctest、pytest、任何构建（AGENTS.md §9；前台提交前亲自编译验证） |
| 本轮**执行** | 逐字读原文复核；在 `/tmp/g08-05-repro/` 用**等价逻辑**（非生产栈）构造复现并实测，含 ASAN |
| 结论 | 越界条件**复核成立**，且比转述更严重（见 §1.2）；已修；同型点全仓排查完成；第二项 4 类 fail-open 逐条处置完毕（改 5 处 / 登记 9 处） |

---

## 1. 越界条件的独立复核

### 1.1 复核结论：**成立**（前台转述的每一句都能在原文逐字对上）

前台转述的四个环节，逐条对上 `lib/algorithms/coverage/src/rejection.cpp`（**行号为整改后**；改前见括号）：

| 环节 | 原文位置 | 复核 |
|---|---|---|
| ① 判定数组尺寸来自资格层，资格层**额外**按「权重非有限」判不合格 | 资格层 `eligibility_core` :1382-1417（改前 :1327-1362），判据调用 :1395-1402；权重门在唯一事实源 :1352-1353（改前 :1341-1345） | ✅ 成立 |
| ② 推进索引的循环**只跳过**值非有限 / 无效标记 / 支持度非正三种 | compat 映射循环 :2429-2436（改前 :2325-2333）：`if (!std::isfinite(in->values[i])) continue;` / `if (in->valid != nullptr && !in->valid[i]) continue;` / `if (in->support != nullptr && !(in->support[i] > 0.0)) continue;` | ✅ 成立 |
| ③ 因此权重非有限的样本被剔除出数组，却仍多推进一次下标 | 资格层产出紧凑数组长度 `m`（:2350-2357，改前 :2299-2306），映射循环的 `k` 初值 0、每个「通过自己那三道门」的样本 `++k`（:2436） | ✅ 成立 |
| ④ 读到尾后字节若等于「已接受」两值之一即静默改判 | 判据面 `dec.reasons` 由 `ScratchVec<std::uint8_t> reasons; reasons.resize(m);` 提供（:2416-2417，长度恰为 `m`）； :2431-2432 读 `dec.reasons[k]`，并把 `P2_REASON_ACCEPTED`(=0) 或 `P2_REASON_UNDERDETERMINED`(=3) 写成 `out->accepted[i]=1` | ✅ 成立 |

越界条件可精确表述为：

> 存在样本 `j` 满足 `isfinite(values[j]) && valid[j] && support[j] > 0 && !isfinite(weights[j])`，
> 且 `j` 位于所有被剔除样本的**最后**一个之后（否则 `k` 只是提前错位）⇒
> 映射循环最后一次迭代 `i = n−1` 时 `k == m` ⇒ 读 `reasons[m]`，越过数组尾。

### 1.2 比转述更严重的两点（前台转述未含，本次复核新增）

**(1) 错位不止一个样本，而是其后**全部**合格样本整体错位一格。**
`t` = 首个权重非有限样本，其后每个合格样本 `s` 都被判成**前一个合格样本**的 reason；
只有最后一个样本落到越界读。⇒ 「静默改判」的面是 `O(n)`，不是一个点。

**(2) 小栈下越界读是「确定性的」，不是「未定义但通常无害」。**
`ScratchVec`（:1017-1058）在 `n <= CAP(=64)` 时 `data()` 指向成员 `std::array<T,64> fixed_{}`
（:1054，`{}` 值初始化 ⇒ 全零）。所以 `m <= 64` 时读 `reasons[m]` **不是 UB，而是读固定数组的
一个零位** ⇒ 恒等于 `P2_REASON_ACCEPTED`(=0) ⇒ **最后一个样本必被静默写成已接受**。
只有在 `m >= 65`（堆路径）时才是真正的堆越界（ASAN 可捕获）。这一条决定了复现方式：小栈复现
**无需依赖堆布局，每次都稳定重现**。

### 1.3 同型「第二份条件」在本仓的存在面（改前）

资格判据在改前共有 **3 份**手写副本，且 1 份已走偏：

| 副本 | 位置（改前） | 状态 |
|---|---|---|
| 连续版 policy core | `eligibility_core` :1327-1362 | 权威（四道门 + 权重门） |
| strided 生产收集器 | `p2_collect_candidate_stack` :1421-1501 | 与权威同条件（逐条对齐，但仍是第二份手写） |
| compat 映射循环 | `p2_reject_stack` :2324-2333 | **漏权重门 → 本单缺陷** |
| compat MIN_SAMPLES 分支 | `p2_reject_stack` :2260-2265（改前 :2259-2265） | 同款三门漏权重门（不越界，但会把不合格样本写成 `accepted=1`） |

---

## 2. 复现（/tmp，等价逻辑，不调用生产栈）

台架：`/tmp/g08-05-repro/repro.cpp`。逐字复刻 `ScratchVec`（:1017-1058）、
`eligibility_core`（:1382-1417）与 compat 映射循环（:2429-2436）；kernel 用**夹具判据**
（紧凑下标奇数 → `REJECTED_HIGH`，偶数 → `ACCEPTED`）代替，与映射逻辑解耦，便于观察错位。

### 2.1 复现输入

```
n = 6，values[i] = 10+i（全有限），valid[i] = 1（全有效），support[i] = 1（全 > 0）
weights = {1, 1, NaN, 1, 1, 1}     ← 只有 weights[2] 非有限
⇒ 资格层：m = 5（样本 2 被剔除），dec.reasons 长度 = 5
```

（`NaN` 权重即「UPM 逐样本 ivar 为 NaN」的等价输入；`Inf` 权重同理走 `kWeightFinite` 门。）

### 2.2 修复前行为（实测输出）

```
---- 修复前 · n=6（ScratchVec 固定路径） ----
n=6  资格层合格数 m=5  尾后读取次数=1
 i   w[i]   资格层  真实判据     k实读  读到的值      accepted  判定
 0   1      合格   ACCEPTED     0      ACCEPTED      1         OK
 1   1      合格   REJECTED_HIGH 1      REJECTED_HIGH 0         OK
 2   nan    剔除   -             2      ACCEPTED      1         X 不合格样本被静默写成已接受
 3   1      合格   ACCEPTED     3      REJECTED_HIGH 0         X 静默改判（紧凑下标错位）
 4   1      合格   REJECTED_HIGH 4      ACCEPTED      1         X 静默改判（紧凑下标错位）
 5   1      合格   ACCEPTED     5      ACCEPTED      1         X 映射错位（本次判据恰好同号）
结论：存在静默改判 / 越界读
```

三点可核对的事实：
1. **样本 2 已被资格层判不合格（剔除），却被写成 `accepted=1`** —— 静默改判；
2. **样本 3/4 的判据整体错位一格**（3 拿到了 4 的 reason，4 拿到了 5 的 reason）—— 静默改判；
3. **样本 5 读的是 `reasons[5]`，数组长度只有 5**（尾后读），读到 `ScratchVec` 固定数组零位
   = `P2_REASON_ACCEPTED` ⇒ 被写成已接受。

堆路径（`n=100`，`weights[40]=NaN`，`m=99 > 64`）的 ASAN 实测：

```
$ ./repro_asan big-before
==2522351==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x50b000000203
READ of size 1 at 0x50b000000203 thread T0
    #0 ... in run /tmp/g08-05-repro/repro.cpp:149      ← r.read[i] = reasons[k]
0x50b000000203 is located 0 bytes after 99-byte region [0x50b0000001a0,0x50b000000203)
allocated by ... std::vector<unsigned char>::resize ... ScratchVec<unsigned char, 64ul>::resize
SUMMARY: AddressSanitizer: heap-buffer-overflow
退出码 1
```

### 2.3 修复后行为（同一台架，同一输入）

```
---- 修复后 · n=6 ----
n=6  资格层合格数 m=5  尾后读取次数=0
 2   nan    剔除   -             0      ACCEPTED      0         OK 不合格样本未判
 3   1      合格   ACCEPTED     2      ACCEPTED      1         OK
 4   1      合格   REJECTED_HIGH 3      REJECTED_HIGH 0         OK
 5   1      合格   ACCEPTED     4      ACCEPTED      1         OK
结论：全部样本判据与资格层一致，无尾后读取

$ ./repro_asan big-after      → 退出码 0，无 sanitizer 报告（n=100 / m=99）
$ ./repro_asan after          → 退出码 0（n=6）
```

---

## 3. 改法：为什么是单一事实源而不是边界钳制

### 3.1 改前 / 改后逐字

**(A) compat 映射循环（核心缺陷）** —— `rejection.cpp` :2429-2436

改前：
```cpp
    std::uint32_t k = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        if (!std::isfinite(in->values[i])) continue;
        if (in->valid != nullptr && !in->valid[i]) continue;
        if (in->support != nullptr && !(in->support[i] > 0.0)) continue;
        if (dec.reasons[k] == P2_REASON_ACCEPTED ||
            dec.reasons[k] == P2_REASON_UNDERDETERMINED)
            out->accepted[i] = 1;
        ++k;
    }
```
改后：
```cpp
    // eligible→original 映射：推进条件 = 资格层逐样本 verdict（单一事实源）。
    // k 恰好在合格样本上推进一次，合格样本数 = m = dec.reasons 的长度 ⇒
    // 读下标恒 < m（k ≤ m-1），结构上不可能读到数组尾之后。
    // 改前此循环自带三道门（漏「权重非有限」），被资格层剔除的样本仍多推进
    // 一次 ⇒ 尾后读；读到 0/3（P2_REASON_ACCEPTED / UNDERDETERMINED）即把该
    // 样本静默写成已接受，并把其后所有样本的判据整体错位一格。
    std::uint32_t k = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        if (!eligible[i]) continue;
        if (dec.reasons[k] == P2_REASON_ACCEPTED ||
            dec.reasons[k] == P2_REASON_UNDERDETERMINED)
            out->accepted[i] = 1;
        ++k;
    }
```

**(B) compat MIN_SAMPLES 分支（同源）** —— :2360-2364：同样的三道门换成 `if (!eligible[i]) continue;`。

**(C) compat 把资格层 verdict 显式要出来** —— :2341-2357：`eligibility_core` 第 12 个出参
`out_eligible` 由 `nullptr` 改为 `eligible.data()`（该出参本就是为此设计的，
`rejection.h:326`「每输入样本 1=合格 0=不合格」）。

**(D) 判据本身抽成唯一事实源** —— 新增 :1337-1376：

```cpp
enum class EligGate {
    kPass, kFinite, kValid, kWeightFinite, kSupport, kQuality
};

inline EligGate eligibility_gate(
    double value, const std::uint8_t* valid_p, const double* weight_p,
    const double* support_p, const std::uint32_t* quality_p,
    double support_threshold, std::uint32_t quality_flags_required) {
    if (!std::isfinite(value)) return EligGate::kFinite;
    if (valid_p != nullptr && *valid_p == 0) return EligGate::kValid;
    if (weight_p != nullptr && !std::isfinite(*weight_p))
        return EligGate::kWeightFinite;
    if (support_p != nullptr && !(*support_p > support_threshold))
        return EligGate::kSupport;
    if (quality_p != nullptr && quality_flags_required != 0 &&
        (*quality_p & quality_flags_required) != quality_flags_required)
        return EligGate::kQuality;
    return EligGate::kPass;
}

inline void eligibility_tally(EligGate g, std::uint32_t* out_finite, ...) {
    switch (g) {
        case EligGate::kFinite:
        case EligGate::kWeightFinite: ++*out_finite; break;
        case EligGate::kValid: ++*out_valid; break;
        case EligGate::kSupport: ++*out_support; break;
        case EligGate::kQuality: ++*out_quality; break;
        case EligGate::kPass: break;
    }
}
```

两条资格实现（`eligibility_core` :1395-1402；`p2_collect_candidate_stack` :1491-1501）
都改为只调 `eligibility_gate` + `eligibility_tally`，判据文字在本 TU 内**只剩这一份**。

### 3.2 为什么不是边界钳制

| 方案 | 为什么被排除 |
|---|---|
| `if (k >= m) break;` / `k = std::min(k, m-1)` | **只把越界读变成错位读**：被剔除样本仍占用一次推进 ⇒ 其后每个合格样本仍被判成前一个样本的 reason（§2.2 第 2 行实测）。根因（两处条件不一致）原封不动，且从此有两套语义（钳制出来的下标 vs 真实下标）需要维护。 |
| `reasons.resize(m+1); reasons[m] = UNDERDETERMINED;` | 越界读消失，但**静默改判被合法化**：给「本不属于候选栈的样本」显式发一张判据，正是缺陷的语义核心（§1.1④）。 |
| 把 `if (k >= m)` 判成 `return 1`（fail-closed 报错） | 症状层面报错，但只要有人再改一次资格层条件就会复发；且根因仍有两份条件。 |
| **本次改法：读资格层自己写下的 verdict** | 「合格与否」只由资格层判一次；映射只做**投影**。两个数组的下标推进天然同构（推进次数 = 合格样本数 = `m` = `reasons` 长度），`k ≤ m−1` 由构造保证，不需要任何边界检查，也不存在需要「再走偏一次」的第二处条件。 |

补充论证（静态可证）：`k` 初值 0，只在 `eligible[i]` 为真的样本上 `++k`，而 `eligibility_core`
返回的合格数恰好等于 `Σ_i eligible[i]`；读取发生在 `++k` 之前 ⇒ 读取下标集合 = `{0,…,m−1}`。

### 3.3 语义守恒声明（对照硬性纪律）

- **没有**放宽阈值、加 epsilon、吞异常：判据的值、顺序、诊断计数逐条不变
  （`eligibility_tally` 与改前的 `++*out_finite / ++*out_valid / ++*out_support / ++*out_quality` 一一对应）。
- **没有**把「静默改判」改成「静默忽略」：不合格样本仍由资格层**判定**为不合格
  （计入 `invalid_finite` 等诊断、`eligible[i]=0`），只是不再被映射成别人的判据；
  它在 `out->accepted[]` 保持 0，与「值非有限样本」的既有处置完全一致。
- **没有**引入第三形态：全栈不合格时 `m=0`，映射循环体一次都不执行（`k` 不被读），
  `p2_reject_stack_ex` 仍按合同走 `MIN_SAMPLES`（:2125），不是静默跳过。

---

## 4. 同型点全仓排查（「一处构造数组、另一处按不同条件推进」）

检索：`grep -rn "source_indices\|src_idx\|eligible_count" --include=*.cpp --include=*.h lib/`
＋对 `p2_collect_candidate_stack` / `p2_eligibility_filter` 的全部 live 消费方逐个读原文。

| # | 位置 | 形态 | 处置 |
|---|---|---|---|
| 1 | `rejection.cpp:2429-2436` compat 映射循环 | **缺陷本体** | **已改**（§3.1A） |
| 2 | `rejection.cpp:2360-2364` compat MIN_SAMPLES 分支 | 同款三门漏权重门；不越界，但把不合格样本写成 `accepted=1` | **已改**（§3.1B） |
| 3 | `rejection.cpp:1472-1518` `p2_collect_candidate_stack` | 资格判据的第二份手写副本（当前条件正确，但属漂移面） | **已改**：改调 `eligibility_gate`，判据只剩一份 |
| 4 | `tools/stage2.cpp:1372`/`:1444-1449` | 紧凑栈消费方：`weights[s]`/`acc[s]` 全程按紧凑下标 `s`，无需回原 slot | 判定**正确**，未改 |
| 5 | `src/acr_kernels.cpp:145`/`:184-188` | 同上，紧凑域自洽 | 判定**正确**，未改 |
| 6 | `module_adapters.cpp:11400-11420`（及 `:11057-11058`） | **用 `src_idx[s]` 显式映射**，代码注释明写「compact index 反推被合同禁用」 | 判定**正确**（正例），未改 |
| 7 | `p2_eligibility_filter`（:1421） | 仅 header 声明 + 归档测试调用，**live 零消费方** | 无需改 |

结论：**live 生产路径（stage2 / acr_kernels / module_adapters）全部走显式 `src_idx` 映射，
本缺陷只在 compat adapter 一处**。整改后全仓「构造紧凑数组」与「按其推进/映射」两侧不再各自
复述条件（3 份副本收敛为 1 份）。

---

## 5. 第二项：同目录其余 fail-open 的处置

### 5.1 （a）对非有限值取绝对值仍非有限 ⇒ 整栈免检且报成功 —— **已改**

**复核成立。** 各方法核把阈值取绝对值后直接进比较：

| 位置（改后） | 表达式 | 阈值 = NaN 的后果 |
|---|---|---|
| `rejection.cpp:1546-1547` `reject_robust_mad_impl` | `lo = -fabs(lower_sigma)`、`hi = fabs(upper_sigma)`；:1578 `z < lo \|\| z > hi` | 两个比较恒假 ⇒ 无样本被判离群 |
| `rejection.cpp:1590-1591` `reject_winsorized_impl` | 同上 | 同上 |
| `rejection.cpp:1650-1651` `reject_averaged_impl` | 同上 | 同上 |
| `rejection.cpp:1707-1708` `reject_linear_fit_impl` | `fit - stack[j] > sigma * siglow` | `sigma*NaN = NaN` ⇒ 恒假 |
| `rejection.cpp:1882-1883` `reject_percentile_impl` | `w[i] < -scale*plow` / `> scale*phigh` | 同上 |
| `rejection.cpp:1907-1908` `reject_median_sigma_impl` | 同 sigma 系 | 同上 |

⇒ `reasons[]` 全写 `ACCEPTED`、零离群、`accepted_count == n`、`status = P2_STATUS_OK`。
可达路径：compat 适配器把调用方的 `sigma_low/sigma_high` 直接 `fabs` 后写进 plan
（:2380-2400，`if (in->sigma_low != 0.0)` 对 NaN 为真 ⇒ 放行），以及任何手工构造 plan 直调
`p2_reject_stack_ex` 的调用方。生产路径（AUTO→`p2_reject_plan_resolve_n`）阈值来自冻结表，
恒有限，故该门**只对手工/外部改写的 plan 触发**，不影响生产。

**改法**（新增 `method_thresholds_finite` :2072-2099 + 入口闸 :2170-2178）：
只校验**本方法实际消费**的连续阈值（sigma 系 / linear_fit / percentile / esd.alpha），
任一非有限 ⇒ 与同族门一致：写满 `reasons = UNDERDETERMINED`、`accepted_count = n`、
`status = P2_STATUS_INVALID_CONFIGURATION`（:2171-2177），调用方对非 {OK, UNDERDETERMINED}
一律 hard fail。

明确排除的两种「伪修」：① 非有限就退回冻结默认阈值（等于把非法 plan 换成另一套未申报的判据）；
② 比较时把 NaN 当「未超阈」（同一缺陷的静默版）。

### 5.2 （b）跳过了必要的一步却返回成功 —— **已改 1 处 / 登记 2 处**

**已改**：`upm.cpp:1084-1101` 数据充分性闸。
复核成立（逐条读原文确认）：`upm.cpp`:1281 `if (!(wi > 0.0) || !isfinite(wi)) continue;`（改前 :1240）
把每条零权重观测剔出判据，:1297 `if (wfk.empty()) continue;` 把空块剔出判据（改前 :1256）；当**全部**观测权重为 0
时，所有块被跳 ⇒ `n_blocks = 0`、`rank_eff = 0`、`n_blocks_rank_deficient = 0`
⇒ （改前 :1328）`identifiable = 1`（判据一步没跑却报绿）+ `chi2 ≡ 0`；
同时 `max_dM < tol_M && max_dC < tol_C`（改前 :1050，改后 :1067）在第一轮就 `converged = 1`；
`build_impl`（改前 :1369，改后 :1411）`return 0` ⇒ 报成功，且 `p2_upm_warnings_json` 两条告警链都不触发。

改法（三处，全部收紧，无新字段、无新返回码）：
1. `:1084-1101` IRLS 结束后加闸：无一观测带「正且有限」权重 ⇒ `p2_upm_close` + `return 2`
   （与 `compute_raw` 既有 rc=2 同族（`upm.cpp:756-761`）；`lib/phase2_session/p2_session.cpp:45-53` 的 `map_rc` 把 rc=2 映成
   `ACS_ERR_STATE`「build fail(production 显式缺 ivar 等)」，注释里的「等」覆盖本情形）；
2. `:1372-1373` `identifiable` 要求 `n_blocks > 0`；`:1374-1375` 零块时 `kappa = +inf`
   （与 `upm.cpp:1356-1357` 秩亏时的既有约定一致；`kappa` 初值 0.0 语义是「完美良态」，不可留在无判据状态）；
3. `:1272`/`:1283-1286`/`:1382-1383` 记 `chi2_terms`，`chi2_red_defined` 要求
   `dof_eff > 0 && chi2_terms > 0`，否则 NaN + defined=0 —— 直接照抄本文件 :1331-1332 自己写下的禁令。

**登记（未擅改）**：

| # | 位置 | 缺陷 | 不改的理由 |
|---|---|---|---|
| R1 | `upm.cpp:2700-2726`（改前）MA 求解器 | Cholesky 连续失败 / 步长全被拒时只 `break`，`P2UpmMaInfo` 无 `converged` 字段，provenance 硬写 `any_fail_closed_reason=""`，把未经拟合的初值当解返回 rc=0 | 修法要新增返回码（正本只冻结 1..8），属返回值合同变更，须负责人裁决 |
| R2 | `sampler.cpp:1055` | `if (neigh.size() < 3) continue;` 连带跳过了**不需要邻域**的 contamination 门（:1066）与保留比例门（:1070） | 修法会改变 mosaic 边缘 tile 的拒绝统计，属科学判据面的跨模块决策；需负责人确认门序 |
| R3 | `coverage.cpp:157-166` | `aio_hips_tile_ipix` 失败条目被静默丢弃，该帧贡献 0 tile 却 `status=0` | 与仓内 `sampler.cpp:390-397`（同类失败判 fail-closed）风格不一致，但不在本单点名目录的 fail-open 语义内，交由 coverage 车道 |

### 5.3 （c）把全部样本剔掉后报「完美拟合」—— **已改 4 处 / 登记 2 处**

**已改 4 处**：

| # | 位置 | 现象 | 改法 |
|---|---|---|---|
| C1 | `sky_plane.cpp:1714` `p2_sky_plane_residuals` | 样本全被过滤 ⇒ `rms_weighted = rms_unweighted = 0.0` + `return OK`，「什么都没算」与「残差为零」不可区分 | `if (used == 0) return P2_SKY_PLANE_NO_USABLE_SAMPLES;`（放在写任何 RMS **之前**，故失败时也不写 0）。依据：`sky_plane.h:511`「rc 同 build 参数约定」＋ build 侧同条件 `:545-549` 已用该码 |
| C2 | `upm.cpp:1382-1383`（+`1272`/`:1285`） | χ² 一项都没进却 `chi2_red = 0` 且 `chi2_red_defined = 1` | 见 §5.2 第 3 点 |
| C3 | `upm.cpp:1372`（+`:1374`） | 零块却 `identifiable = 1`、`kappa = 0.0` | 见 §5.2 第 2 点 |
| C4 | `upm.cpp:1084` | 全部权重为 0 仍 `return 0` 产出零模型 | 见 §5.2 第 1 点 |

**登记（未擅改）**：

| # | 位置 | 缺陷 | 不改的理由 |
|---|---|---|---|
| R4 | `sky_plane.cpp:1208` | `info.chi2_red = (dof_eff > 0.0) ? (swr2/dof_eff) : 0.0;` —— 无自由度时把「无定义」写成「残差 0」，**直接违反 `upm.cpp:1379-1380` 的明文禁令**；连带 `:1451-1452` 自适应细化的 `info.chi2_red > 0.0` 探针恒假 ⇒ 细化步骤被静默跳过仍 `return OK` | 正本修法要动 `P2SkyPlaneInfo` 冻结布局（加 `chi2_red_defined`）并改 `p2_sky_plane_save/open` 的 JSON 往返与 `build_adaptive` 的判定条件 ⇒ 跨文件合同变更，超出本单单文件授权，须负责人裁决。**已给出逐行修法草案**（见 §6） |
| R5 | `rejection.cpp:2255-2270` | `n <= 4` 且方法核全拒时把 `reasons` 全改成 `UNDERDETERMINED`、`accepted_count = n`、两个 rejected 计数清零，`status = UNDERDETERMINED` | **正本已明文冻结该容错域**（`docs/science/REJECTION.md` §4:49-51「全拒容错域：n = 4 ∧ 方法核全拒 ⇒ 降级 UNDERDETERMINED 全接受」并给出可达域三条件），代码内亦有冻结理由（改前 :2200-2206，改后 :2228-2234）。属 owner 裁定 + 正本条款，**本单不改**；仅登记一处合同瑕疵：清零 `rejected_low/high` 与 `rejection.h:395` 「低于 lower threshold」的计数定义不符（建议后续由负责人裁定是否只删这两行） |

### 5.4 （d）连续量缺有限性校验 ⇒ 母版变非有限而仍返回成功 —— **已改 2 处 / 登记 3 处**

先更正前台线索的两点（复核结论）：
1. `lib/algorithms/coverage/` 全仓 `grep -i master` **零命中**；本仓「母版」在 coverage 层指
   **UPM 公共面 M + 逐帧校正场 C**（`PHASE2_UPM.md` §叠加恒等式）与 **sky-plane 的 B_ref + δ_k**
   （`sky_plane.h:460`）。下文按此叙述。
2. 「**负值**」在多数命中里不成立：守卫多写成 `x <= 0.0`（与 NaN 比较恒假）⇒ **NaN 穿透**；
   真正的负值在本仓几乎都被 `fabs`/`clamp` 先吸收。共同根因：
   `std::max(a,b)` 编译为 `(a<b)?b:a`，**NaN 在第一位时返回 NaN** ⇒「取下限」这个动作本身
   在 NaN 下**反向失效**。唯一货真价实的「负值」路径是 D2。

**已改 D1（UPM 观测入口）** —— `upm.cpp:259-275`：
`build_impl` 全文 28 处 `isfinite` **无一落在观测入口**；而姊妹求解器 `p2_upm_ma_build`
（`upm.cpp:2509-2510`）逐条校验。漏检路径（已逐行走通）：
`obs.uncertainty = NaN` ⇒ `sigma_eff = max(fabs(NaN), sigma_floor) = NaN`（下限反向失效）
⇒ `huber_w(NaN)` ⇒ `w[i] = NaN` ⇒ 法方程 `rhs/obs_w = NaN` ⇒ `cg_solve_frame` 的
`pAp <= 1e-30` 对 NaN **判假** ⇒ `m->C[f] = NaN`（`calibrate_block` 输出 `input−NaN`，rc=0）；
`obs.value = NaN`（w 有限时）⇒ `den > 1e-12` 仍为真 ⇒ `M[k] = NaN` ⇒ 进 `model_hash` 成产品。
**合法域已查正本**：`upm.h:39-40`「`value` …（**可负**）」/「`uncertainty` …标准误」⇒
`value` 可负但不可非有限、`uncertainty` ∈ [0,+∞) 且必须有限；
`docs/science/PHASE2_UPM.md:163-165`「`sigma_eff = max(|uncertainty|, sigma_floor)`」+ `:173-174`
「sigma_floor 的角色是**下限**」。改法：在任何分配之前一次性校验 `value`/`uncertainty`，
非有限 ⇒ `return 2`（同 `compute_raw` rc=2 语义，无需释放）。

**已改 D2（sky-plane 模型文件几何量）** —— `sky_plane.cpp:2009-2029`：
`p2_sky_plane_open` 只校验尺寸就 `return OK`，不校验 `h`/`us`/`vs`：
- `us/vs == 0` ⇒ `delta_basis` 的 `(u−uc)/us = ±Inf` ⇒ `pow(Inf,a) = Inf` ⇒ **母版天光面非有限**；
- `h == 0` ⇒ `bspline_basis` 的 `floor((u−t0)/h) = ±Inf` ⇒ `(int)±Inf` 是 **UB**；
- **`us/vs < 0`（负值）** ⇒ `x` 整体反号 ⇒ δ 多项式奇偶项关系反转 ⇒ `corrected_k = raw_k − δ_k`
  施加**反向**天光扣除 ⇒ 帧间台阶不消反增，而 rc 全程 0。

**合法域已查正本且 build 侧已实现**：`sky_plane.cpp:499`（`node_spacing_deg > 0` 硬约束）、
`:660-662`（`us/vs = max(0.5*(umax−umin), 1e-9)`）⇒ 合法域 = (0,+∞) 且有限；
`docs/science/PHASE2_UPM.md` 节点间距导出条款「几何量缺失 ⇒ 显式失败，不回退常数」；
`DATA_SEMANTICS.md:1748`「域外（含 NaN）一律回退…**域外 clamp 会把一个与帧无关的表值当成逐帧标定**」。
改法：open 侧补 `h/us/vs` 的正性+有限性校验与 `coeff`/`deltas` 的有限性校验，
任一不满足 ⇒ `delete m; return P2_SKY_PLANE_IO_ERROR`（与同函数既有失败码一致）。
唯一 live 调用方 `lib/infrastructure/scheduler/src/module_adapters.cpp:10502` 对非 0 已 fail-closed。

**已改 D1'（sky-plane 求值入口）** —— `sky_plane.cpp:1547-1554` 与 `:1619-1622`
（`p2_sky_plane_eval` / `p2_sky_plane_eval_delta`）：`ra/dec` 非有限 ⇒ `gnomonic` 的
`cosc <= 1e-12` 对 NaN 判假 ⇒ `u/v = NaN` ⇒ 越域守卫同样全判假 ⇒
`*out_value = NaN` 而 `out_status = P2_SKY_EVAL_OK`。**位置非法不是越域**，改按
`P2_SKY_EVAL_INVALID`（该枚举本就是两函数入口的初值，此前从未被写出）报，rc 仍 0 =
「调用方按状态处理」（`sky_plane.h:461`）。仓内四处同语义已有三种正确写法（`sky_plane.cpp:536` build 侧、`:352` `p2_star_mask_contains`、`sampler.cpp:1337`），本处是唯一漏检的。

**登记（未擅改）**：

| # | 位置 | 缺陷 | 不改的理由 |
|---|---|---|---|
| R6 | `upm.cpp:1995-2013` `p2_upm_raw_weight` legacy 分支 | `sigma_floor/support_power` 的 NaN 洞 + `unc = max(fabs(uncertainty), sigma_floor)` ⇒ `*out_raw` 为 NaN/Inf 却 `return 0`；下游 `compute_raw` 把该 control 权重**整体清零**（已是「静默跳过」第三形态） | 正本（`upm.h:180-183`）只冻结返回码 0/1/2，新增码须同步 `upm.h` 与 `PHASE2_UPM_IMPL.md` F1 |
| R7 | `sampler.cpp:538`/`:894-899` | `cfg.control_k_corr <= 0.0` 漏 NaN ⇒ `cvar/civar/unc` 非有限而 `return 0` | 配置面要同步 `stage2_common.cpp` 解析面与 `module_adapters.cpp:9558`，跨 3 文件；产出面的「无尺度信息」记法要按 `docs/science/PHASE2_UPM.md:107` 既有口径新写 |
| R8 | `sampler.cpp:103` `kcorr_lookup` | `std::clamp(pixfrac, 0.5, 1.0)` 把域外值静默饱和；`docs/science/DATA_SEMANTICS.md:1748` 逐字点名「域外 clamp 会把一个与帧无关的表值当成逐帧标定」 | 正本方向明确（应回退 1.4），但改后 `control_variance` 会系统性上移 5–14%，属**科学判据变更**，须负责人裁决并重跑相关实验 |

---

## 6. 「若编译不过最可能的几处」

按可能性排序（本轮**未编译**，以下为静态自证的薄弱点）：

1. **`rejection.cpp:1491-1499`（最可能）** —— `eligibility_gate` 的第 2 实参写成三元
   `in->valid != nullptr ? &in->valid[...] : nullptr`。`nullptr`（`std::nullptr_t`）与
   `const std::uint8_t*` 的条件表达式在 C++11 起合法，但若某个老标准模式（`-std=c++98`）
   或严格 MSVC 版本对「指针 vs nullptr_t」求公共类型报错，会在这里失败。
   已在整改过程中**自查出并修掉过一次同类错误**（曾写 `&vv` 而 `vv` 已被删除 ⇒ 悬空标识符），
   说明这类点确实会咬人。若报错，最小改法：把三元拆成 `const std::uint8_t* vp = nullptr;
   if (in->valid != nullptr) vp = &in->valid[...];` 再传 `vp`。
2. **`rejection.cpp:1364-1376` `eligibility_tally` 的 `switch`** —— 覆盖全部 6 个枚举值但无
   `default`。若某编译器开了 `-Wswitch-enum` 之外的变体或把 `enum class` 底层类型推导异常会报；
   反之若将来给 `EligGate` 加枚举值且忘记补 case，`-Wswitch` 会红（这是**期望行为**，不是缺陷）。
3. **`rejection.cpp:2072-2099` `method_thresholds_finite`** —— `P2SigmaParams` 四个成员
   （`sigma/winsorized/averaged/median_sigma`）已对照 `rejection.h:215-221` 逐字核过；
   `method` 是 `int`、`P2_REJECT_*` 是枚举常量，比较无窄化。若 `P2_REJECT_*` 未落进本 TU
   的作用域（rejection.h 顶层 enum，必然可见），会报未声明。
4. **`upm.cpp:259-275` / `:1084-1101`** —— `obs[i].value` / `obs[i].uncertainty` 字段名已对照
   `upm.h:39-40` 核过；`p2_upm_close((void*)m)` 与既有 :748 同款调用；`w` 是
   `std::vector<double>`（:730 声明，早于 :1081）。若 `p2_upm_close` 未在 upm.h 声明会在此报
   未声明——但同函数 :748 已用它，故必然已声明。
5. **`sky_plane.cpp:2009-2029`** —— `m->coeff`（`std::vector<double>`）与 `m->deltas`
   （`std::vector<std::vector<double>>`，由既有的 `dk.size() != m->m` 反证）上的
   range-for；`std::isfinite` 需 `<cmath>`（本文件已大量使用）。变量名 `cv`/`dv` 与外层
   无冲突（项目未开 `-Wshadow`）。
6. **`-Wconversion` 敏感点（已主动规避）** —— 项目对自有生产 target 开了
   `-Wall -Wextra -Wpedantic -Wconversion`（`CMakeLists.txt:1349/1359/1362`）与 MSVC `/W4`。
   `:1488-1490` 的 `qv` 用 `0u`（`unsigned int` → `std::uint32_t`，同宽无窄化）；
   `wv/sv` 用 `0.0`（同型）。**曾经打算写成 `: 1u` 给 `std::uint8_t vv`（条件表达式提升到
   `int`），已改为直接传元素地址以彻底消除该转换**。若前台仍见窄化告警，最可能在这里。
7. **跨文件契约检查（不是编译错，是行为回归）** —— 本轮新增的三个 fail-closed 出口会让
   此前「静默成功」的输入变成失败：
   `p2_upm_build` rc=2（唯一 live 调用方 `lib/phase2_session/p2_session.cpp:220-224` 已 fail-closed）、
   `p2_sky_plane_residuals` rc=2（**live 零调用方**）、
   `p2_sky_plane_open` rc=`P2_SKY_PLANE_IO_ERROR`（唯一 live 调用方 `module_adapters.cpp:10502`
   已 fail-closed）。若前台 e2e 变红，请按「这是预期的 fail-closed 收紧」核对输入，
   不要靠放宽门来「修绿」。

---

## 7. 自证段

**S1 越界条件**：逐字读过原文并把四个环节的 `文件:行` 列在 §1.1；用 `/tmp` 等价台架复现出
**尾后读 1 次 + 1 个不合格样本被写成已接受 + 2 个合格样本判据错位**（§2.2），
堆路径另有 ASAN `heap-buffer-overflow` 实测（READ of size 1，0 bytes after 99-byte region）。

**S2 修复后**：同一台架、同一输入 → 尾后读 0 次、错位 0 处、ASAN 退出码 0（§2.3）。

**S3 单一事实源而非钳制**：给出了四条候选修法的排除理由（§3.2），并给出构造性证明
`k ∈ {0,…,m−1}`（推进次数 = 合格样本数 = `m` = `reasons` 长度）；判据文字在本 TU 内由 3 份
收敛为 1 份（§3.1D、§4）。

**S4 同型点**：`grep source_indices|src_idx|eligible_count` 全仓扫描 + 逐个读三个 live 消费方，
确认生产路径全部走显式 `src_idx` 映射（§4 表格第 4–6 行）。

**S5 第二项**：4 类形态逐条给出「文件:行 + 原文 + 为什么会报成功 + 改法」；
改 5 处（阈值有限性闸、compat 两处映射、UPM 数据充分性闸 + 两处判决位、sky residuals 空集、
sky eval 两处 + open 一处），登记 8 处并逐条写明**为什么没改**（正本冻结 / 需新返回码 /
跨文件合同变更 / 科学判据变更）。

**S6 纪律自检**：无阈值放宽、无 epsilon、无吞异常；无一处把 fail-open 改成「静默跳过」；
`p2_upm_build` 那一处是把**既存的静默跳过**（`compute_raw` 把污染 control 权重整体清零）
**收回**到显式失败；所有新增出口都是具名非 0 / INVALID 状态，调用方一律 fail-closed。

**S7 未执行项（如实登记）**：项目编译、ctest、pytest、任何构建**均未执行**（AGENTS.md §9）。
静态自证的薄弱点与最可能的编译失败处列在 §6。另：`/tmp/g08-05-repro/` 的台架是
**等价逻辑复刻**（逐字抄 `ScratchVec`/`eligibility_core`/映射循环，kernel 用夹具判据），
不是生产栈实跑——它证明的是控制流与索引关系，不替代前台对真实目标的编译与测试。

**S8 需负责人裁决（不在本单授权内）**：R4（`sky_plane.cpp` 的 `chi2_red` 需动冻结布局，
修法草案：照抄 `upm.cpp:1382-1390` 的 NaN+`chi2_red_defined` 模式 ⇒ `P2SkyPlaneInfo` 加
`int chi2_red_defined`；`p2_sky_plane_save` 改用既有 `num_or_null` 写 null；
`build_adaptive:1451-1452` 的探针条件先判双方 defined，不可判定时不据此决策而非判「无改善」）、
R5、R1、R2、R6、R7、R8。