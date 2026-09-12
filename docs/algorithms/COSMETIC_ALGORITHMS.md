# Cosmetic Correction Algorithms (P1-COS)

> ID 覆盖: ALG-COS-001..005  状态: CONTRACT_READY (P1-COS-DOC 冻结, 2026-09-07)  上游: SCI-CAL-001  下游: DATA-P1-COS / API-COS-001 / API-P1-002 / TEST-COS-DESIGN-001
> 本文档由源码逐函数核对后重写（P1-COS-DOC，wave W1）。实现唯一生产源 =
> `lib/calibration/src/cosmetic_corrector.cpp`（CMake 目标
> `astrocs_calibration`，CMakeLists.txt:373-380；C ABI 导出
> `lib/calibration/src/ac_api.cpp:108,228`，签名权威
> `lib/calibration/include/astro_calibration.h:97,142`）；迁移目标目录
> `lib/cosmetic/`（落码由 P1-COS-IMPL 执行，尚未存在生产符号）。
> 科学定义见 `docs/science/CALIBRATION.md`（SCI-CAL-001，FROZEN，§2 参数表
> `hot_sigma/cold_sigma/method/max_structure_size`、§6 假设、§9a mask 极性
> 1=坏点）。本文档只登记离散算法与实现事实，不修改 SCI；算法分层与
> ALG-CAL-004 的重叠界定见 §0。禁止声明 IMPLEMENTED。

## 0 范围与 ALG-CAL-004 重叠界定

SCI-CAL-001 §12 将坏点检测/修复登记为 `ALG-CAL-004`（引用遗留符号
cc_detect_hot/cold + cc_correct_median）。本模块按迁移矩阵独立冻结为
ALG-COS-001..005，逐公式锚定 **现行唯一生产实现**
`ac::detect_hot_pixels/detect_cold_pixels/filter_by_structure_size/
interpolate_pixels/correct_frame`（cosmetic_corrector.cpp:61-265，经
`ac_correct_frame`/`ac_correct_frame_f64` 导出）：

- ALG-CAL-004（CALIBRATION_ALGORITHMS.md）与 ALG-COS-001..005 描述同一
  现行生产实现，ALG-CAL-004 为 P1-CAL 合同视角的摘要引用；本模块文档是
  cosmetic 域逐公式的权威登记（P1-COS-IMPL 迁移落码、P1-COS-TEST 落测试
  均以本文档为准）。
- 遗留 cc_* 通道（`lib/calibration/cpp/cosmetic_corrector.cpp`：
  cc_detect_hot/cc_detect_cold/cc_correct_median/cc_last_error）**未编译
  进 CMake 主构建**，仅作为计划迁移旧符号登记（见 §8 与 module.yaml），
  其公式（局部窗口中值修复、window 3..15）与生产 ac_* 通道不同。

## 1 ALG-COS-001 全局阈值检测（热/冷像素）

- 源锚: `lib/calibration/src/cosmetic_corrector.cpp:118-137`（detect_hot_pixels）、`139-158`（detect_cold_pixels）。
- 输入: 检测源帧 `src`（热检测=master dark，冷检测=master bias；float32，
  ADU，`[h][w]` 行主序 0-based），sigma 倍数 `threshold_sigma`
  （无量纲），结构尺寸上限 `max_size`（像素个数）。
- 统计量（对整帧逐像素，不过滤 NaN/Inf——见 DISP-COS-002）:
  - `med = median(src)`（`compute_global_median`，cosmetic_corrector.cpp:45-48，
    复制后 `std::nth_element`，O(n)，偶数长度取双中位均值——见 ALG-COS-005）；
  - `mad = median(|src − med|)`（`compute_global_mad`，cosmetic_corrector.cpp:50-54）；
  - `sigma = 1.4826 · mad`（高斯假设换算系数，SCI-CAL-001 §9；`1.4826f`）。
- 判定（离散公式，逐像素 i）:
  - 热: `hot_mask[i] = (dark[i] > med_d + hot_sigma · σ_d) ? 1 : 0`
    （cosmetic_corrector.cpp:126-128；严格大于）。
  - 冷: `cold_mask[i] = (bias[i] < med_b − cold_sigma · σ_b) ? 1 : 0`
    （cosmetic_corrector.cpp:147-149；严格小于）。
- 边界/前置: `dark==NULL` 或 `n=w·h<=0` → 直接返回，mask **保持调用方
  原值不清零**（cosmetic_corrector.cpp:121-123）；`mad=0`（无离散度）时
  `sigma=0`，阈值退化为 `±med`（检测不禁止）。
- 并行: 判定循环 `#pragma omp parallel for schedule(static)`
  （cosmetic_corrector.cpp:126,147），逐像素独立、bitwise 确定性；统计
  （median/MAD）在主线程串行。并行轴=像素域。
- 确定性: 判定与统计与线程数无关（无跨像素归约顺序差异；
  `compute_global_mad` 的 `sum += fabsf(...)` 求和为串行固定顺序）。

## 2 ALG-COS-002 连通域结构过滤（8 邻接）

- 源锚: `lib/calibration/src/cosmetic_corrector.cpp:61-113`（filter_by_structure_size）。
- 输入: 候选掩码 `mask`（char，1=坏点候选），`max_size`（保留连通域
  尺寸上限）。
- 连通性: 8 邻接（dx∈{-1,0,1}, dy∈{-1,0,1}，含对角；cosmetic_corrector.cpp:74-86）。
- 离散步骤:（cosmetic_corrector.cpp:64-111）
  1. `std::vector<int> labels(n, 0)`；BFS 队列 `std::queue<int>`；
  2. 逐像素扫描：`mask[i]!=1` 或已标号则跳过；否则 BFS 标记该连通域
     （label 自增），记录 `sizes[label]`；
  3. 过滤：`sizes[label] >= max_size` 的连通域全部清零
     （cosmetic_corrector.cpp:95-101）——即**保留小连通域（孤立坏点/小
     团簇），剔除大连通域**（SCI-CAL-001 §6 假设：坏点稀疏、与天体源
     不混淆，大结构按非坏点处理）；
  4. 背景 label 0 的 size 记为 `max_size`（cosmetic_corrector.cpp:70），
     保证 `0 >= max_size` 不成立、非候选像素永不被误清（遗留通道曾出现
     背景 label 泄漏 bug，本实现已修复——lib/calibration/memory.md
     2026-07-10 记录）。
- 复杂度: O(n)（每像素至多入队一次）+ O(n) 额外内存
  （labels + sizes 向量，int×n）。
- 并行: 判定/清理阶段 `#pragma omp parallel for schedule(static)`
  （cosmetic_corrector.cpp:107-110，清零循环）；**BFS 标记主线程串行**
  （有共享可变 labels/queue）。并行轴=像素域（清零段）。
- 确定性: 标号顺序与遍历顺序固定（行主序 i 升序），过滤结果与线程数无关。
- 缺陷引用: 全帧坏点时 O(n) 整型额外内存 ×2（labels+sizes）无上限防护
  ——上限即帧大小，登记为 DISP-COS-006 关联事实。

## 3 ALG-COS-003 插值修复（method=0 中值 / method=1 名义 bilinear）

- 源锚: `lib/calibration/src/cosmetic_corrector.cpp:160-227`（interpolate_pixels）。
- 输入: 数据帧 `data`（float32 ADU）、掩码 `bad_mask`（1=坏点，SCI-CAL-001
  §9a）、`method`（0=AC_METHOD_MEDIAN, 1=AC_METHOD_BILINEAR，
  astro_calibration.h:18-19）；输出 `out`（调用方分配）。
- 通用规则（逐像素 i，`#pragma omp parallel for schedule(static)`，
  cosmetic_corrector.cpp:166）:
  - 非坏点: `out[i] = data[i]`（逐像素拷贝）；
  - 空邻域回退: `out[i] = data[i]`（原值保留，不置 NaN）。
- method=0（median，5×5 窗口，cosmetic_corrector.cpp:174-199）:
  - 邻域: 5×5（dy,dx∈[-2,2]），**镜像反射边界**
    （`nx<0→-nx`、`nx>=w→2w-nx-2`，再 clamp 到 [0,w-1]；
    cosmetic_corrector.cpp:180-190）——小帧（w<3 或 h<3）镜像可能自映射，
    邻域重复计数由中值天然免疫（中值不因重复计数偏移，仅样本数变化）；
  - 仅收集 `bad_mask[nidx]==0` 的邻居（掩码内好像素）；
  - `out[i] = median(vals)`（`median_inplace`，`std::nth_element`，
    cosmetic_corrector.cpp:34-43；**偶数个样本取双中位均值**
    `(hi+lo)*0.5f`，其中 hi=v[n/2]、lo=下半区 max_element——见
    ALG-COS-005）；
  - `vals` 为空（5×5 全坏/邻域全坏）→ 原值保留（cosmetic_corrector.cpp:196-198）。
- method=1（AC_METHOD_BILINEAR，**名义 bilinear 实为 4 方向 IDW**，
  cosmetic_corrector.cpp:201-224；缺陷登记 DISP-COS-003）:
  - 4 个正交方向 d∈{(−1,0),(1,0),(0,−1),(0,1)}；每方向自 (x,y) 起步沿
    d 步进，跳过连续坏点直到首个好像素或出界（`while bad_mask` 循环，
    cosmetic_corrector.cpp:208-212），`dist` = 步进像素数（起步=1）；
  - 出界方向不贡献；好像素贡献 `w_d = 1/dist`（**反比距离加权，非
    bilinear 权重**；float 单精度）；
  - `out[i] = Σ(w_d·data[n_d]) / Σ w_d`（cosmetic_corrector.cpp:218-219）；
  - `Σ w_d == 0`（四方向全部出界或全坏）→ 原值保留（cosmetic_corrector.cpp:220-222）。
- 语义后果（登记事实）: 对单像素坏点，IDW = 四邻好像素的 (1,1,1,1) 等权
  平均（dist 全为 1）≠ bilinear 的角点权重；对拉长坏点结构（被
  ALG-COS-002 保留的小团簇），各方向 dist 不同产生方向性偏差。
  该行为是**现行冻结行为**，SCI 未规定具体插值核（SCI §2 仅登记 method
  参数名）；P1-COS-TEST 按本文档公式冻结容差，**不得**以"修正为真
  bilinear"为由改公式（若将来修正需 SCI/控制包变更）。
- 并行/确定性: 逐像素独立，线程数无关；float 累加顺序 = 方向数组固定顺序
  d=0..3（确定性）。
- 内存: 每像素临时 `std::vector<float> vals`（≤25 float，method=0），
  RAII，峰值 O(25) 每工作线程。

## 4 ALG-COS-004 帧级编排（correct_frame 主管线）

- 源锚: `lib/calibration/src/cosmetic_corrector.cpp:229-265`（ac::correct_frame）。
- 离散步骤:
  1. 参数语义: `data/w/h/out` 必需；`n=w·h`（int 乘法，无溢出防护，
     DISP-COS-005）；
  2. 热检测: **当且仅当** `dark != NULL && hot_sigma > 0`
     （cosmetic_corrector.cpp:243-247）→ detect_hot_pixels（ALG-COS-001），
     否则 `hot_mask` 全 0（`std::vector<char> hot(n,0)` 先零初始化）；
  3. 冷检测: 当且仅当 `bias != NULL && cold_sigma > 0`
     （cosmetic_corrector.cpp:248-252）→ detect_cold_pixels（ALG-COS-001）；
  4. 合并: `all_bad[i] = hot_mask[i] | cold_mask[i]`（bitwise or，
     cosmetic_corrector.cpp:254-256，omp 并行）；
  5. 插值: interpolate_pixels(data, all_bad, ..., method)
     （ALG-COS-003）；
  6. 计数输出: `out_hot = Σ(all_bad && hot_mask)`、`out_cold =
     Σ(all_bad && cold_mask)`（omp parallel + reduction(+:hot,cold)，
     cosmetic_corrector.cpp:258-264）——即**结构过滤后**（ALG-COS-002
     已剔除大连通域）且未因插值回退改变的坏点计数；`out_hot/out_cold`
     可为 NULL（不输出，调用点判空）。
- 关键编排语义（生产事实，禁止美化）:
  - `dark==NULL` 或 `hot_sigma<=0` → 热检测关闭；`bias==NULL` 或
    `cold_sigma<=0` → 冷检测关闭；两者皆关 → `all_bad` 全 0，
    interpolate_pixels 逐像素拷贝（恒等映射），**模块退化为空转 pass**；
  - **现网生产调用点** `lib/phase1_session/p1_session.cpp:294-301`
    （cosmetic stage，2026-09-01 c5629be6 引入）以
    `master_dark=nullptr, master_bias=nullptr` 调用（两检测全禁用），
    `out_hot/out_cold=0`，帧逐像素原样复制回写
    （p1_session.cpp:305-307 memcpy 后 aio_write_fits）——即**当前生产
    cosmetic 阶段从未执行过真实检测/修复**（no fabrication of valid
    coverage：不做假修复；检测生效需 P1-COS-IMPL/配置接线母版）。
  - 输出可等于输入缓冲? 现行 API 无别名约束登记（out 由调用方分配；
    in-place `data==out` 未定义，见 §7 DATA-P1-COS invalid 行）。
- 返回值: ac::correct_frame 无返回值；C ABI 层 `ac_correct_frame`
  （ac_api.cpp:108-122）参数校验 `!data||!out||width<=0||height<=0` →
  `AC_ERR_PARAM(-1)`，否则恒 `AC_OK(0)`；`AC_ERR_MEMORY(-2)/
  AC_ERR_INTERNAL(-3)` **定义但从未返回**（无 extern "C" 异常屏障，
  bad_alloc 可穿越 C ABI；DISP-COS-001）。
- 取消: 无取消检查点（PHASE1_API_V1 §2 登记 cosmetic 取消点=无；session
  层帧粒度取消覆盖整帧粒度）。API-P1-002 契约值。
- FP64 ABI: `ac_correct_frame_f64`（ac_api.cpp:228-263）double 输入→
  float 降级→执行 f32 实现→double 输出（统计/mask/插值全程 float32；
  astro_calibration.h:105-115 注释声明）——f64 ABI 不是真双精度
  （DISP-COS-004）。

## 5 ALG-COS-005 统计基元（median / MAD 换算）

- 源锚: `lib/calibration/src/cosmetic_corrector.cpp:34-54`（匿名命名空间）。
- `median_inplace(std::vector<float>&)`（:34-43）: `std::nth_element`
  就位中位数，O(n) 期望；**奇数 n** = `v[n/2]`；**偶数 n** =
  `(hi + lo) * 0.5f`，其中 `hi = v[n/2]`（nth_element 后上中位）、
  `lo = *max_element(v.begin(), v.begin()+n/2)`（**下半区 max_element
  取下中位**，非第二次 nth_element）；空向量（n<=0）返回 `0.0f`
  （调用点 vals 为空时走原值回退分支，:196-198，不依赖该返回值）。
- `compute_global_median(const float*, int)`（:45-48）: 复制全帧 →
  median_inplace；额外内存 O(n) float。
- `compute_global_mad(const float*, int, float med)`（:50-54）: 复制
  `absdev[i] = fabsf(data[i] − med)` 到向量后取中位（O(n) 时间 + O(n)
  内存）；float 单精度减法+fabsf。`σ = 1.4826f · mad`（高斯假设，
  SCI-CAL-001 §9）。
  若 mad=0（常量帧）→ σ=0，阈值=±med（ALG-COS-001 行为不变）。
- `median_inplace` 为比较选择（非稳定），但**中位值本身**由顺序统计量
  唯一确定（偶数分支 hi/lo 两元素也唯一）→ 结果与实现内部顺序
  无关、与线程数无关；float 语义逐位可复现。
- 复杂度: O(n) 时间 + O(n) 额外内存（每次检测复制一份）。

## 6 复杂度、确定性、并行归约汇总

| 项 | 结论 | 源锚 |
|---|---|---|
| 时间 | 检测 O(n)+排序选择；过滤 O(n)；插值 O(n)（median 分支每坏点 O(25)，IDW 分支每坏点 O(坏点游程)）；总计 O(n) | §1-§3 |
| 额外内存 | 检测 O(n)（统计复制+labels）；插值每线程 O(25)；f64 转接层 O(n) 全帧复制 | §4 |
| 并行轴 | 像素域 omp parallel for schedule(static)（判定/合并/清零/插值/计数）；统计与 BFS 串行 | §1-§4 |
| 归约 | 仅 out_hot/out_cold 计数归约（omp reduction(+)，顺序不确定但整数加法可交换→结果确定） | §4 |
| 确定性 | 输出 bitwise 与线程数无关（逐像素独立 + 固定遍历顺序 + 中位/IDW 无跨像素顺序耦合） | §1-§5 |
| 线程数控制 | 进程级 OpenMP ICV（ac_set_num_threads 可改写；现状调用点 p1_session 由 budget 注入——DISP-COS-008 整改点） | §4 |

## 7 误差来源与数值精度

- 全程 float32（f64 ABI 同样降级，DISP-COS-004）：检测阈值比较、
  IDW 权重 `1/dist`、双中位均值 `(hi+lo)*0.5f` 均单精度。
- 误差来源: ① MAD 高斯假设（非高斯分布时阈值偏移，SCI §9 已声明）；
  ② 偶数样本双中位均值（hi=上半区最小/lo=下半区最大）与"真中位数"
  定义差异（§5）；③ IDW 方向性偏差（§3，DISP-COS-003）；④ NaN 传播：检测统计不过滤 NaN——若检测源帧
  含 NaN，`data[i] > threshold` 等比较为 false → NaN 不判坏，
  `median_inplace` 排序中 NaN 行为实现定义（std::nth_element 用 operator<，
  NaN 使排序结果未指定）→ 中位数可能非数值 → 阈值 NaN → 整帧判定
  全 false（hot）或全 false（cold）→ **NaN 输入可使检测静默失效**
  （DISP-COS-002；负面测试必须覆盖）。
- 无 fast-math（CMake 主构建，CMakeLists.txt astrocs_calibration 无
  相关 flag；遗留 MinGW 通道除外，非生产）。

## 8 遗留通道与迁移旧符号（非生产）

- `lib/calibration/cpp/cosmetic_corrector.{cpp,h}`: cc_* 通道
  （cc_correct_median[data,bad_mask,H,W,window 奇数 3..15]/cc_detect_hot/
  cc_detect_cold/cc_last_error；局部窗口中值修复，非全局阈值检测），
  经 Makefile 编译为 cosmetic_corrector.dll；**未编译进 CMake 主构建**
  （lib/calibration/README.md §9）。属计划迁移旧符号，公式与 ALG-COS-001..005
  不同，不得作为现状依据。
- 迁移落点: `lib/cosmetic/`（P1-COS-IMPL 建 astrocs_p1_cosmetic.dll +
  C ABI adapter + plan/execute/cancel/inspect + ThreadLease 接线）；
  本 DOC 不改任何生产代码。

## 9 测试设计 TEST-COS-DESIGN-001（冻结容差）

> 可执行测试由 P1-COS-TEST 建立（TEST-P1-COS-001 目标 ID）；本节冻结
> fixture/oracle/容差，P1-COS-TEST 不得放宽。全离线合成数据，零真实
> 数据依赖。负面行必须逐条断言（ALG §1-§4 + DATA-P1-COS invalid 列）。

- **FIX-COS-A 常量场**: data=常量 C、dark/bias=常量 → mad=0、无坏点、
  out==data（bitwise 恒等）；`out_hot=out_cold=0`。
  容差: bitwise 相等（max_abs==0，解析）。
- **FIX-COS-B 解析注入坏点**: 常量场注入 k 个单像素坏点（含角点/边线/
  中心，镜像边界与 IDW 路径分别命中），独立 oracle = NumPy 复算
  ALG-COS-001 判定 + §3 公式（median 双中位均值、IDW 权重 1/dist）。
  容差: NumPy 对照 float32 rtol=1e-6, atol=1e-7（沿用 SCI-CAL-001 §11
  预冻结标度）；掩码/计数精确相等。
- **FIX-COS-C 连通域结构**: 注入 L 形 5 像素、2 像素对、孤立单点
  （max_size=4 → 5 像素域被剔除保留原值，2 像素域保留修复）；
  scipy.ndimage.label(8 连通) 复算连通域与 sizes。
  容差: 掩码精确相等；8 连通语义独立 oracle。
- **FIX-COS-D NaN/Inf 注入**: data 或检测源帧注入 NaN/Inf →
  **必须按 DISP-COS-002 断言现状行为**（NaN 不判坏；NaN 源帧使检测
  全 false）；不得断言"未定义"。
  容差: 行为断言（bitwise/布尔），非数值容差。
- **FIX-COS-E IDW 方向性**: 竖直 3 像素坏点列 → 修复值 = 上下好像素
  等权均值（dist=1,1; 左右 dist=2,2 → 权重 0.5,0.5）——解析解逐位
  断言（float32 表达式按 §3 顺序）。
  容差: bitwise（同序 float 算术确定）。
- **FIX-COS-F 双中位均值**: 偶数样本邻居计数（如 5×5 窗内 24 好像素）
  → median = (hi+lo)*0.5 解析值（hi=上中位、lo=下半区最大）。
  容差: bitwise。
- **不变量 I1-I6**: ①非坏点逐像素恒等；②无检测条件恒等（dark/bias
  NULL 或 sigma<=0 → out==data bitwise）；③掩码极性 1=坏点（SCI §9a）；
  ④确定性（1/2/4 线程 bitwise 一致）；⑤计数=过滤后坏点数（≤掩码和）；
  ⑥空邻域回退原值。
- **负面/参数矩阵**: NULL data/out、w<=0/h<=0 → AC_ERR_PARAM（C ABI 层）；
  out_hot/out_cold NULL 不崩溃；method 非法值（2）→ 按 §3 else 分支
  走 IDW（**现状：非 0 即 IDW**，cosmetic_corrector.cpp:174 `if
  (method == AC_METHOD_MEDIAN)`，负面断言现状）；max_size<=0 → 全部
  连通域 size>=max_size 清零（全域清除语义，负面断言现状）。
- **串并行/资源**: 1/2/4 线程 bitwise 一致；heavy run CPU/RSS 监控、
  内存上限断言（O(n) 常数界）；无嵌套并行。
- **ISA**: 基线标量断言（无 SIMD 变体；引入时按约束 C.4-C.8 逐内核
  benchmark 冻结 ULP 容差）。

## 10 缺陷清单（DISP-COS，登记不改码）

> 均为**现行实现事实**，迁移整改由 P1-COS-IMPL/INT 处理；本 DOC 阶段
> 禁止据此修改生产代码。编号与 ALG-CAL §10（DISP-CAL-001..011）独立。

| ID | 缺陷（现状事实） | 锚 |
|---|---|---|
| DISP-COS-001 | 无 extern "C" 异常屏障：std::bad_alloc/omp 异常可穿越 C ABI；AC_ERR_MEMORY(-2)/AC_ERR_INTERNAL(-3) 定义但从未返回（死值错误码） | ac_api.cpp:108-122,228-263；astro_calibration.h:23-24 |
| DISP-COS-002 | 检测统计不过滤 NaN/Inf：NaN 源帧 → 中位数/阈值非数值 → 判定静默全 false；NaN 数据像素不判坏直接透传 | cosmetic_corrector.cpp:45-54,126-128,147-149 |
| DISP-COS-003 | method=1（AC_METHOD_BILINEAR）名义 bilinear 实为 4 方向 1/dist IDW；非 0 method 一律走 IDW（无参数校验） | cosmetic_corrector.cpp:174,201-224；astro_calibration.h:19 |
| DISP-COS-004 | ac_correct_frame_f64 非真双精度：double→float 降级执行（统计/mask/插值全程 f32），仅 I/O 层 double | ac_api.cpp:228-263；astro_calibration.h:105-115 |
| DISP-COS-005 | `n = w·h` int 乘法无溢出防护（int31 域）；w·h>2^31 行为未定义 | cosmetic_corrector.cpp:231；ac_api.cpp:108-122 |
| DISP-COS-006 | 检测/过滤 O(n) 额外内存（统计复制 + labels/sizes 向量）无上限防护（上限=帧大小，登记为常数界） | cosmetic_corrector.cpp:45-48,64-111 |
| DISP-COS-007 | 无取消检查点（PHASE1_API_V1 §2 cosmetic 取消点=无；session 帧粒度取消为替代粒度） | cosmetic_corrector.cpp:229-265；PHASE1_API_V1 §2 |
| DISP-COS-008 | 并行=OpenMP 进程级默认 team（ICV 可被 ac_set_num_threads 全局改写），不满足 ThreadLease 约束 D.3/D.4——迁移整改点 | cosmetic_corrector.cpp:126,147,166,254,258 |
| DISP-COS-009 | 生产调用链现状未接线母版：p1_session.cpp:296-297 传 nullptr dark/bias → 检测全禁用、模块空转（检测从未在生产生效） | p1_session.cpp:294-307 |
| DISP-COS-010 | in-place（data==out 别名）未定义且未校验；out 与 data 重叠区域行为未登记 | ac_api.cpp:108-122 |
| DISP-COS-011 | 镜像反射边界在 w<3/h<3 小帧下邻域自映射（重复计数；中值免疫偏移但 IDW 出界方向被 clamp 语义吸收），小帧语义未在 SCI 声明 | cosmetic_corrector.cpp:180-193 |

## 11 关联

- SCI: SCI-CAL-001（docs/science/CALIBRATION.md，FROZEN；§2 参数表、
  §6 坏点稀疏假设、§9a mask 极性 1=坏点、§11 oracle 容差标度）。
- DATA: DATA-P1-COS（docs/contracts/DATA_SEMANTICS.md §10）；上游输入
  DATA-P1-CAL（§9）。
- API: API-COS-001（docs/contracts/PUBLIC_API.md，ac_correct_frame/
  ac_correct_frame_f64/ac_set_num_threads）；API-P1-002
  （docs/api/PHASE1_API_V1.md §2，编排合同，多模块共享）。
- MOD/SRC: MOD-astrocs-phase1-cosmetic（lib/cosmetic/module.yaml，
  CONTRACT_READY；lib/cosmetic/README.md 实现事实）。
- TEST: TEST-COS-DESIGN-001（本文档 §9）；可执行 TEST-P1-COS-001 由
  P1-COS-TEST 建立。
- 摘要引用: ALG-CAL-004（docs/algorithms/CALIBRATION_ALGORITHMS.md §3.4，
  P1-CAL 合同视角同一实现）。
