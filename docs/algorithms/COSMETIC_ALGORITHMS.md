# Cosmetic Correction Algorithms (P1-COS)

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

> ID 覆盖: ALG-COS-001..005  状态: CONTRACT_READY  上游: SCI-CAL-001  下游: DATA-P1-COS / API-COS-001 / API-P1-002 / TEST-COS-DESIGN-001
> 实现唯一生产源 =
> `lib/algorithms/calibration/src/cosmetic_corrector.cpp`（CMake 目标
> `astrocs_calibration`（源清单 `lib/algorithms/calibration/CMakeLists.txt:20-25`）；C ABI 导出
> `lib/algorithms/calibration/src/ac_api.cpp:108,228`，签名权威
> `lib/algorithms/calibration/include/astro_calibration.h:97,142`）；迁移目标目录
> `lib/algorithms/cosmetic/`（落码由 P1-COS-IMPL 执行，尚未存在生产符号）。
> 科学定义见 `docs/science/CALIBRATION.md`（SCI-CAL-001，FROZEN，§2 参数表
> `hot_sigma/cold_sigma/method/max_structure_size`、§6 假设、§9a mask 极性
> 1=坏点）。本文档只登记离散算法与实现事实，不修改 SCI；算法分层与
> ALG-CAL-004 的重叠界定见 §0。禁止声明 IMPLEMENTED。

## 0 范围与 ALG-CAL-004 重叠界定

SCI-CAL-001 §12 将坏点检测/修复登记为 `ALG-CAL-004`（引用 cc_* 通道符号
cc_detect_hot/cold + cc_correct_median）。本模块按迁移矩阵独立冻结为
ALG-COS-001..005，逐公式锚定 **现行唯一生产实现**
`ac::detect_hot_pixels/detect_cold_pixels/filter_by_structure_size/
interpolate_pixels/correct_frame`（cosmetic_corrector.cpp:61-265，经
`ac_correct_frame`/`ac_correct_frame_f64` 导出）：

- ALG-CAL-004（CALIBRATION_ALGORITHMS.md）与 ALG-COS-001..005 描述同一
  现行生产实现，ALG-CAL-004 为 P1-CAL 合同视角的摘要引用；本模块文档是
  cosmetic 域逐公式的权威登记（P1-COS-IMPL 迁移落码、P1-COS-TEST 落测试
  均以本文档为准）。
- **单一生产源（冻结）**：本文档全部 `文件:行` 锚点在不写全路径时一律指
  `lib/algorithms/calibration/src/cosmetic_corrector.cpp`（CMake 目标
  `astrocs_calibration` 与 `astrocs_p1_calibration` 的生产源；
  `lib/algorithms/calibration/CMakeLists.txt:20-25` 的 `CAL_PROD_SOURCES`
  与根 `CMakeLists.txt` 的 `add_library(astrocs_calibration STATIC ...)`
  源清单均含它）；
  同名文件 `lib/algorithms/calibration/cpp/cosmetic_corrector.cpp` **已退役**，
  登记见 §8。二者是两套独立实现，**禁止**用 cpp/ 版本的公式、阈值或退化语义
  解释或复算本模块行为。
- 退役通道 cc_*（`lib/algorithms/calibration/cpp/cosmetic_corrector.{cpp,h}`：
  cc_detect_hot/cc_detect_cold/cc_correct_median/cc_last_error）**不在任何 CMake
  目标内**（唯一构建路径是同目录 Windows MinGW `Makefile`，产物
  `cosmetic_corrector.dll`，仓内无消费者），见 §8。

## 1 ALG-COS-001 全局阈值检测（热/冷像素）

- 源锚: `lib/algorithms/calibration/src/cosmetic_corrector.cpp:119-135`（detect_hot_pixels，函数定义 :119）、`:140-156`（detect_cold_pixels，函数定义 :140）。
- 输入: 检测源帧 `src`（热检测=master dark，冷检测=master bias；float32，
  ADU，`[h][w]` 行主序 0-based），sigma 倍数 `threshold_sigma`
  （无量纲），结构尺寸上限 `max_size`（像素个数）。
- **标度与量纲（冻结，逐项）**：`threshold_sigma` 无量纲；`med`/`mad`/`σ`/阈值与
  `src` 同标度（ADU 域定义见 `docs/science/CALIBRATION.md` §3：
  `物理值 = BSCALE·样本 + BZERO`）。
  - **检出集合对 `src` 的正标度变换严格不变**：`med`/`σ`/阈值三者同步缩放，
  比较 `src[i] > med + threshold_sigma·σ` 在 `c>0` 下等价；结构过滤只依赖
  掩码的连通性，同样不变。**实测（直接调用生产 `ac::detect_hot_pixels`）**：
  同一 32×32 帧与其 ×(1/65535) 副本检出数均为 3、掩码**逐像素 0 处不同**；
  负对照（撤掉其中 1 个热像素）掩码有 1 像素不同 ⇒ 该不变性判据非恒真。
  ⇒ **"标度错使检测结果完全改变"不成立**；`src` 的绝对标度**不是**本层的
  正确性前提，本层也不得据此声称做过标度校验。
  - **真正的域要求**：`src` 与 `data`/`bad_mask` **必须同域**——掩码是像素集合
  （与标度无关），但产物 `out` 的非坏点像素逐位拷贝自 `data`，坏点像素由
  `data` 的邻域插值而来 ⇒ **`out` 的标度 = `data` 的标度**。若 `data` 不在
  `cal` 合同域（`calibrated_adu`）内，产物标度即违约；该判定归消费边界，
  不归本层。
  - **唯一已知会使检出集合失真的机制是 `mad=0` 退化**（见下条适用域），
  与标度无关。
- 统计量（对整帧逐像素，不过滤 NaN/Inf——见 DISP-COS-002）:
  - `med = median(src)`（`compute_global_median`，cosmetic_corrector.cpp:46-48，
    复制后 `std::nth_element`，O(n)，偶数长度取双中位均值——见 ALG-COS-005）；
  - `mad = median(|src − med|)`（`compute_global_mad`，cosmetic_corrector.cpp:51-54）；
  - `sigma = 1.482602218505602 · mad`（高斯假设换算系数，单精度域：代码写作 `1.482602218505602f`，即该全精度字面量的 float 舍入，与双精度相对差 **+1.36e-08**；SCI-CAL-001 §9、与 SCI-NOISE-001 §14.2 同值）。
- 判定（离散公式，逐像素 i）:
  - 热: `hot_mask[i] = (dark[i] > med_d + hot_sigma · σ_d) ? 1 : 0`
    （阈值赋值 cosmetic_corrector.cpp:127、判定比较 :131；**严格大于**）。
  - 冷: `cold_mask[i] = (bias[i] < med_b − cold_sigma · σ_b) ? 1 : 0`
    （阈值赋值 cosmetic_corrector.cpp:148、判定比较 :152；**严格小于**）。
- 边界/前置: `dark==NULL` 或 `n=w·h<=0` → 直接返回，mask **保持调用方
  原值不清零**（cosmetic_corrector.cpp:121-122）。
- **适用域（`mad=0` 退化，冻结判据）**：`mad = median(|src − med|) = 0` ⇔
  至少半数像素恰等于中位数。此时 `σ = 0`、阈值退化为 `med`（热）/`med`（冷），
  判定变成"**是否严格大于/小于中位数**"，与坏点无关。**实测（驱动生产静态库
  `astrocs_calibration` 的 `ac::detect_hot_pixels` / `ac::correct_frame`）**：
  32×32 帧、80% 像素为 100.0、20%（205 px）为孤立的 100+50.0 ⇒ 检出 205/205
  （真值 0），且 `ac::correct_frame` 实际改写 205/1024 像素；对照组同尺寸帧注入
  3 个孤立热像素（+200/+300/+400 ADU，帧内 σ≈7.08 ADU）⇒ 检出恰 3 个。
  ⇒ **本检测的成立前提是检测源帧有非零稳健尺度（`mad>0`）**；`mad=0` 时
  检出数由"与中位不等的像素占比"决定，**不是坏点率**，该结果不得作为坏点
  统计或 QA 指标使用。调用方必须在 `mad=0` 时显式降级登记（拒绝或标记
  "检测不可用"），不得把此时的计数当真值消费。
- 并行: 判定循环 `#pragma omp parallel for schedule(static)`
  （cosmetic_corrector.cpp:126,147），逐像素独立、bitwise 确定性；统计
  （median/MAD）在主线程串行。并行轴=像素域。
- 确定性: 判定与统计与线程数无关（无跨像素归约顺序差异；
  `compute_global_mad` 的 `sum += fabsf(...)` 求和为串行固定顺序）。

## 2 ALG-COS-002 连通域结构过滤（8 邻接）

- 源锚: `lib/algorithms/calibration/src/cosmetic_corrector.cpp:61-113`（filter_by_structure_size）。
- 输入: 候选掩码 `mask`（char，1=坏点候选），`max_size`（保留连通域
  尺寸上限）。
- 连通性: 8 邻接（dx∈{-1,0,1}, dy∈{-1,0,1}，含对角；邻居展开循环 cosmetic_corrector.cpp:85-97）。
- 离散步骤:（cosmetic_corrector.cpp:64-111）
  1. `std::vector<int> labels(n, 0)`；BFS 队列 `std::queue<int>`；
  2. 逐像素扫描：`mask[i]!=1` 或已标号则跳过；否则 BFS 标记该连通域
     （label 自增），记录 `sizes[label]`；
  3. 过滤：`sizes[label] >= max_size` 的连通域全部清零
     （尺寸统计 cosmetic_corrector.cpp:103-106、清零循环 :108-113）——即**保留小连通域（孤立坏点/小
     团簇），剔除大连通域**（SCI-CAL-001 §6 假设：坏点稀疏、与天体源
     不混淆，大结构按非坏点处理）；
  4. 背景（非候选像素）`labels[i] == 0`：`sizes` 数组只对 `labels[i] > 0` 的
     像素累加（cosmetic_corrector.cpp:104-106），故 `sizes[0]` 恒为初值 **0**；
     清零循环再以 `labels[i] > 0` 为前提（cosmetic_corrector.cpp:110），
     **背景像素在任何 `max_size` 取值下都不会被清零**（含 `max_size<=0`）。
     机制是"标号 0 被两处 `labels[i] > 0` 跳过"，不是"背景 size 记为
     `max_size`"。
- 复杂度: O(n)（每像素至多入队一次）+ O(n) 额外内存
  （labels + sizes 向量，int×n）。
- 并行: 判定/清理阶段 `#pragma omp parallel for schedule(static)`
  （cosmetic_corrector.cpp:108-110，清零循环）；**BFS 标记主线程串行**
  （有共享可变 labels/queue）。并行轴=像素域（清零段）。
- 确定性: 标号顺序与遍历顺序固定（行主序 i 升序），过滤结果与线程数无关。
- 缺陷引用: 全帧坏点时 O(n) 整型额外内存 ×2（labels+sizes）无上限防护
  ——上限即帧大小，登记为 DISP-COS-006 关联事实。

## 3 ALG-COS-003 插值修复（method=0 中值 / method=1 名义 bilinear）

- 源锚: `lib/algorithms/calibration/src/cosmetic_corrector.cpp:160-226`（interpolate_pixels）。
- 输入: 数据帧 `data`（float32 ADU）、掩码 `bad_mask`（**1=坏点**，SCI-CAL-001
  §9a）、`method`（0=AC_METHOD_MEDIAN, 1=AC_METHOD_BILINEAR，
  `lib/algorithms/calibration/include/astro_calibration.h:23-24`）；
  输出 `out`（调用方分配）。
- **极性口径（冻结，跨模块对照）**：本模块 `bad_mask`/`hot_mask`/`cold_mask`
  一律 **1 = 坏点（需修复）**；叠加 rejection 层的 `accepted` 掩码极性**相反**
  （1 = 被接受）。跨模块传递掩码时**必须**显式转换，禁止直接复用同一缓冲区；
  本层不校验极性、也不推断极性，极性错的表现是"修好像素、保留坏像素"。
- 通用规则（逐像素 i，`#pragma omp parallel for schedule(static)`，
  cosmetic_corrector.cpp:166）:
  - 非坏点: `out[i] = data[i]`（逐像素拷贝）；
  - 空邻域回退: `out[i] = data[i]`（原值保留，不置 NaN）。
- method=0（median，5×5 窗口，cosmetic_corrector.cpp:175-199）:
  - 邻域: 5×5（dy,dx∈[-2,2]），**镜像反射边界**
    （`nx<0→-nx`、`nx>=w→2w-nx-2`，再 clamp 到 [0,w-1]；
    cosmetic_corrector.cpp:180-190）——小帧（w<3 或 h<3）镜像可能自映射，
    邻域重复计数由中值天然免疫（中值不因重复计数偏移，仅样本数变化）；
  - 仅收集 `bad_mask[nidx]==0` 的邻居（掩码内好像素）；
  - `out[i] = median(vals)`（`median_inplace`，`std::nth_element`，
    cosmetic_corrector.cpp:35-43；**偶数个样本取双中位均值**
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
  - `out[i] = Σ(w_d·data[n_d]) / Σ w_d`（cosmetic_corrector.cpp:219-220；`sum_w>0` 判据 :219）；
  - `Σ w_d == 0`（四方向全部出界或全坏）→ 原值保留（cosmetic_corrector.cpp:220-222）。
- 语义后果（登记事实）: 对**孤立单像素**坏点，四方向 dist 恒为 1 ⇒ 权重恒为
  (1,1,1,1) ⇒ 结果 = **四正交邻像素的等权算术平均**；该值恰好也是"沿 x/y
  两方向对四正交邻像素做线性插值"的双线性口径在同一位置的值，故**孤立点上
  二者数值相同**，差别只在合同名（DISP-COS-003）。口径差异出现在
  **非孤立**情形：对拉长/团簇坏点结构（被 ALG-COS-002 保留的小连通域），
  各方向 dist 不等，`1/dist` 权重使远处好像素权重下降，且**完全不含对角
  方向**，因而与"以四对角像素等权平均"或"对直接邻像素做双线性"都不同，
  在斜向坏点链上产生方向性偏差（量级随结构长度增长）。
  **插值核的规范状态（冻结）**：SCI-CAL-001 §2 只登记 `method` 参数名，未规定
  插值核的数学形式；本模块的**唯一**插值核即本节两条（method=0 5×5 镜像反射
  中值、method=1 四方向 `1/dist` IDW），P1-COS-TEST 按本节公式冻结容差。
  **要改插值核**（含把 method=1 改成真 bilinear）必须先改 SCI-CAL-001 §2 的
  `method` 定义并走 ALG 变更流程——实现不得单方面偏离本节公式，测试也不得
  按未冻结的核写 oracle。**要求**：任何 `method` 取值都必须落到本节两条之一，
  且核的适用域（孤立点 / 拉长结构 / 小帧）按本节声明，不得留未定义分支。
- 并行/确定性: 逐像素独立，线程数无关；float 累加顺序 = 方向数组固定顺序
  d=0..3（确定性）。
- 内存: 每像素临时 `std::vector<float> vals`（≤25 float，method=0），
  RAII，峰值 O(25) 每工作线程。

## 4 ALG-COS-004 帧级编排（correct_frame 主管线）

- 源锚: `lib/algorithms/calibration/src/cosmetic_corrector.cpp:229-265`（ac::correct_frame）。
- 离散步骤:
  1. 参数语义: `data/w/h/out` 必需；`n=w·h`（int 乘法，无溢出防护，
     DISP-COS-005）；
  2. 热检测: **当且仅当** `dark != NULL && hot_sigma > 0`
     （cosmetic_corrector.cpp:243-247）→ detect_hot_pixels（ALG-COS-001），
     否则 `hot_mask` 全 0（`std::vector<char> hot(n,0)` 先零初始化）。
     **禁用必须显式登记（冻结）**：`hot_sigma<=0` 与 `dark==NULL` 都是
     "该类检测关闭"的语义，本层不产生错误码、不产生退化标志；调用方**必须**
     把"检测源未接线/阈值置零"写入 manifest 与产物 provenance，并在
     `out_hot==0 && out_cold==0` 时**不得**把该帧记为"已做坏点修复"
     （0 计数与"检测关闭"不可区分，见 §4 生产现状与 §9 非退化伴随断言）；
  3. 冷检测: 当且仅当 `bias != NULL && cold_sigma > 0`
     （cosmetic_corrector.cpp:248-252）→ detect_cold_pixels（ALG-COS-001）；
  4. 合并: `all_bad[i] = hot_mask[i] | cold_mask[i]`（bitwise or，
     cosmetic_corrector.cpp:254-256，omp 并行）；
  5. 插值: interpolate_pixels(data, all_bad, ..., method)
     （ALG-COS-003）；
  6. 计数输出: `out_hot = Σ hot_mask`、`out_cold = Σ cold_mask`
     （cosmetic_corrector.cpp:255-259,264-265，单线程 O(n) 计数循环，
     **不是** omp reduction；因 `all_bad = hot | cold`，`Σ(all_bad && hot_mask)`
     与 `Σ hot_mask` 恒等）——计数是**结构过滤后**（ALG-COS-002 已剔除
     `size >= max_size` 的连通域）的掩码像素数，与插值是否回退原值无关。
     **量纲/单位**：计数为无量纲像素个数，适用域 = "检出并保留的候选坏点数"；
     **不适用**于坏点率、探测器坏点表或 QA 指标（`max_size` 与 `mad` 都会
     改变它，见 §1 适用域与 §9）。`out_hot/out_cold` 可为 NULL（不输出，
     调用点判空）。
- 关键编排语义（生产事实，禁止美化）:
  - `dark==NULL` 或 `hot_sigma<=0` → 热检测关闭；`bias==NULL` 或
    `cold_sigma<=0` → 冷检测关闭；两者皆关 → `all_bad` 全 0，
    interpolate_pixels 逐像素拷贝（恒等映射），**模块退化为空转 pass**；
  - **现网生产调用点（两处，均传 NULL 检测源）**：
    ① legacy session 路径 `lib/phase1_session/p1_session.cpp:462-465`
    （cosmetic stage，`ac_correct_frame(image_px, w, h, nullptr, nullptr, ...)`），
    逐像素原样复制回写（`p1_session.cpp:471` memcpy → `:472` `aio_write_fits`），
    计数写 manifest（`p1_session.cpp:477-478`）；
    ② 调度器路径 `lib/infrastructure/scheduler/src/module_adapters.cpp:2355`
    （`p1_op_cosmetic`，同样 `nullptr, nullptr`），`:2364` memcpy 回写、`:2370` 原子写盘、`:2394-2395` 计数写 manifest。
    ⇒ **两条生产路径的 `hot_sigma/cold_sigma` 都 >0（默认 5.0）而检测源为
    NULL，故热/冷检测全部关闭，`out_hot=out_cold=0`，模块退化为恒等 pass**
    （no fabrication of valid coverage：不做假修复；检测生效需接线母版）。
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

- 源锚: `lib/algorithms/calibration/src/cosmetic_corrector.cpp:35-54`（匿名命名空间）。
- `median_inplace(std::vector<float>&)`（:35-44）: `std::nth_element`
  就位中位数，O(n) 期望；**奇数 n** = `v[n/2]`；**偶数 n** =
  `(hi + lo) * 0.5f`，其中 `hi = v[n/2]`（nth_element 后上中位）、
  `lo = *max_element(v.begin(), v.begin()+n/2)`（**下半区 max_element
  取下中位**，非第二次 nth_element）；空向量（n<=0）返回 `0.0f`
  （调用点 vals 为空时走原值回退分支，:196-198，不依赖该返回值）。
- `compute_global_median(const float*, int)`（:45-48）: 复制全帧 →
  median_inplace；额外内存 O(n) float。
- `compute_global_mad(const float*, int, float med)`（:50-54）: 复制
  `absdev[i] = fabsf(data[i] − med)` 到向量后取中位（O(n) 时间 + O(n)
  内存）；float 单精度减法+fabsf。`σ = 1.482602218505602f · mad`（高斯假设，全精度字面量在 float 域舍入到 1.4826022，
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
  （DISP-COS-002；负面测试必须覆盖）。**要求（冻结）**：检测源帧或数据帧
  含 NaN/Inf 时，调用方**必须**在进入本层前拒绝该帧或显式降级登记
  （"检测不可用"），**不得**把"阈值 NaN ⇒ 判定全 false ⇒ 0 检出"当真值消费；
  本层自身的 NaN 行为按上列现状断言，不构成"已处理 NaN"的保证。
- 无 fast-math（CMake 主构建，CMakeLists.txt astrocs_calibration 无
  相关 flag；非生产 MinGW 通道除外）。

## 8 非生产通道与待迁移符号

- **`lib/algorithms/calibration/cpp/cosmetic_corrector.{cpp,h}` — 已退役（登记）**：
  cc_* 通道（cc_correct_median[data,bad_mask,H,W,window 奇数 3..15]/
  cc_detect_hot/cc_detect_cold/cc_last_error），唯一构建路径是同目录 Windows
  MinGW `Makefile`（产物 `cosmetic_corrector.dll`）；**不在任何 CMake 目标
  内**（`lib/algorithms/calibration/CMakeLists.txt:20-25` 的 `CAL_PROD_SOURCES`
  与根 `CMakeLists.txt` 的 `astrocs_calibration` 源清单都不含它），仓内无消费者，`lib/algorithms/calibration/python/` 与
  `cosmetic_corrector.dll` 在当前树中不存在。**退役理由（逐条实测事实，非风格差异）**：
  | 项 | 生产源 `src/cosmetic_corrector.cpp` | 退役源 `cpp/cosmetic_corrector.cpp` |
  |---|---|---|
  | `mad=0` 分支 | `σ=0` ⇒ 阈值退化为 `med`（§1 适用域） | `mad<=0` ⇒ **回退总体标准差**（:229-237,:299-307） |
  | 边界处理 | 5×5 **镜像反射**（:180-190） | 窗口 **clamp**（:163-166） |
  | 窗口 | 固定 5×5 | 参数化 3..15 奇数（:122-129） |
  | 并行 | `schedule(static)` | `schedule(dynamic,64)` + reduction（:153） |
  | 邻域源 | 原帧 `data` | 备份副本 `backup`（:147,:172） |
  | 全局统计 | 单线程 median/MAD | 同口径但偶数样本用两次 `nth_element`（:39-45，数值等价） |
  ⇒ 二者在**退化语义与边界语义上给出不同数值**，属两套独立实现。退役源
  **禁止**被引用为现状依据、禁止被新测试选为 oracle；如需其能力（参数化
  窗口）应迁入生产源并走 ALG 变更流程，不得反向以退役源为准。
- 迁移落点: `lib/algorithms/cosmetic/`（P1-COS-IMPL 建 astrocs_p1_cosmetic.dll +
  C ABI adapter + plan/execute/cancel/inspect + ThreadLease 接线）；
  本文档不改任何生产代码。

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
  预冻结标度；`rtol` 无量纲、`atol` 与 `data` 同标度 = ADU）；掩码/计数
  精确相等（无量纲像素个数）。**适用域（冻结）**：本容差组界定的是
  "同一输入下实现 vs NumPy oracle 的算术自洽性"，**对检测源标度错完全不
  敏感**——源帧整体缩放时实现与"按同错标度写的 oracle"仍逐位一致，而掩码
  会整体改变。⇒ 本容差组**不得**作为"标度正确"或"检测有效"的证据；
  后者由 §1 标度前置与 §9 非退化伴随断言承担。
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
- **不变量②的非退化伴随断言（冻结）**：②在"检测关闭"时恒真，**单独使用
  不具备证据资格**——两条生产路径当前正是 dark/bias=NULL（§4），故仅有②时
  测试全绿而模块从未行使检测分支。②**必须**与下列成对断言同组执行，缺一
  即该组无证据资格：(a) **检测开启时必须有非零效应**：同一 data、同一
  `hot_sigma>0`，注入 k 个孤立超阈像素（k < `max_size`）后
  `out != data` 且 `out_hot == k`（正例，见 §1 实测 P4 对照组：注入 3 ⇒ 检出 3）；
  (b) **真值无效应时归零**：`hot_sigma<=0` 或 `dark==NULL` ⇒ `out==data` 且
  `out_hot==0`（②本身）；(c) **可执行负例**：把 `correct_frame` 的检测调用
  短路后 (a) 必须判红。判据的判别力由 (a) 与 (b) 给出**不同结果**证明。
- **负面/参数矩阵**: NULL data/out、w<=0/h<=0 → AC_ERR_PARAM（C ABI 层）；
  out_hot/out_cold NULL 不崩溃；max_size<=0 → **候选掩码被整片清空 ⇒ 修复
  空转、out 与 data 逐位相同**（`sizes[label] >= 0` 恒真 ⇒ 每个候选连通域
  都清零；**不是**"把整帧当坏点"）。实测（直接调用生产 `ac::correct_frame`）：32×32 帧
  3 像素坏点列，`max_size=0` ⇒ `out_hot=0`、改写 0/256；`max_size=4` 对照
  ⇒ `out_hot=3`、改写 3/256。
- **`method` 词表的三层口径（冻结，逐层给适用域）**：
  ① `ac::correct_frame`/`interpolate_pixels` 层：`method == AC_METHOD_MEDIAN(0)`
  走 5×5 中值，**其余一切取值**（含 1、2、负数）走 IDW 分支
  （cosmetic_corrector.cpp:175 `if`/`:202 else`）——**实测** `method=2` 与
  `method=1` 输出 256/256 像素逐位相同（实测同上）；
  ② legacy session 路径：`p1_session.cpp:445-450` 对词表外取值
  **显式失败**（`ACS_ERR_PARAM`，fail-closed）；
  ③ 调度器路径：`module_adapters.cpp:2328-2329` 对词表外取值**静默回落 median**
  （fail-open）。⇒ **词表外取值在生产两条路径上都不走 IDW 分支**；①的
  "非 0 即 IDW"是模块内事实，**不得**当作生产行为描述。③与②互斥，登记
  DISP-COS-012。
- **串并行/资源**: 1/2/4 线程 bitwise 一致；heavy run CPU/RSS 监控、
  内存上限断言（O(n) 常数界）；无嵌套并行。
- **ISA**: 基线标量断言（无 SIMD 变体；引入时按约束 C.4-C.8 逐内核
  benchmark 冻结 ULP 容差）。

## 10 缺陷清单（DISP-COS，登记不改码）

> 均为**现行实现事实**，迁移整改由 P1-COS-IMPL/INT 处理；本文档不改
> 生产代码。编号与 ALG-CAL §10（DISP-CAL-001..011）独立。

| ID | 缺陷（现状事实） | 锚 |
|---|---|---|
| DISP-COS-001 | 无 extern "C" 异常屏障：std::bad_alloc/omp 异常可穿越 C ABI；AC_ERR_MEMORY(-2)/AC_ERR_INTERNAL(-3) 定义但从未返回（死值错误码） | ac_api.cpp:108-122,228-263；astro_calibration.h:23-24 |
| DISP-COS-002 | 检测统计不过滤 NaN/Inf：NaN 源帧 → 中位数/阈值非数值 → 判定静默全 false；NaN 数据像素不判坏直接透传 | cosmetic_corrector.cpp:46-54,126-128,147-149 |
| DISP-COS-003 | method=1（AC_METHOD_BILINEAR）名义 bilinear 实为 4 方向 1/dist IDW；模块层非 0 method 一律走 IDW（无参数校验） | `lib/algorithms/calibration/src/cosmetic_corrector.cpp:175,202-224`；`lib/algorithms/calibration/include/astro_calibration.h:23-24` |
| DISP-COS-004 | ac_correct_frame_f64 非真双精度：double→float 降级执行（统计/mask/插值全程 f32），仅 I/O 层 double | ac_api.cpp:228-263；astro_calibration.h:105-115 |
| DISP-COS-005 | `n = w·h` int 乘法无溢出防护（int31 域）；w·h>2^31 行为未定义 | cosmetic_corrector.cpp:231；ac_api.cpp:108-122 |
| DISP-COS-006 | 检测/过滤 O(n) 额外内存（统计复制 + labels/sizes 向量）无上限防护（上限=帧大小，登记为常数界） | cosmetic_corrector.cpp:46-48,64-111 |
| DISP-COS-007 | 无取消检查点（PHASE1_API_V1 §2 cosmetic 取消点=无；session 帧粒度取消为替代粒度） | cosmetic_corrector.cpp:229-265；PHASE1_API_V1 §2 |
| DISP-COS-008 | 并行=OpenMP 进程级默认 team（ICV 可被 ac_set_num_threads 全局改写），不满足 ThreadLease 约束 D.3/D.4——迁移整改点 | cosmetic_corrector.cpp:126,147,166,254,258 |
| DISP-COS-009 | 生产调用链现状未接线母版：两条生产路径均传 nullptr dark/bias → 检测全禁用、模块空转（检测从未在生产生效） | `lib/phase1_session/p1_session.cpp:462-465`（计数 :477-478、回写 :471-472）；`lib/infrastructure/scheduler/src/module_adapters.cpp:2355`（回写 :2364） |
| DISP-COS-010 | in-place（data==out 别名）未定义且未校验；out 与 data 重叠区域行为未登记 | ac_api.cpp:108-122 |
| DISP-COS-011 | 镜像反射边界在 w<3/h<3 小帧下邻域自映射（重复计数；中值免疫偏移但 IDW 出界方向被 clamp 语义吸收），小帧语义未在 SCI 声明 | cosmetic_corrector.cpp:180-193 |
| DISP-COS-012 | `method` 词表外取值的两条生产路径处置互斥：`p1_session.cpp:445-450` 显式失败（fail-closed），`module_adapters.cpp:2328-2329` 静默回落 median（fail-open，即已从 session 路径移除的旧形态）；同一配置在两条路径上得到不同产物，且 fail-open 侧无诊断 | `module_adapters.cpp:2328-2329`；对照 `p1_session.cpp:440-450` |

## 11 关联

- SCI: SCI-CAL-001（docs/science/CALIBRATION.md，FROZEN；§2 参数表、
  §6 坏点稀疏假设、§9a mask 极性 1=坏点、§11 oracle 容差标度）。
- DATA: DATA-P1-COS（docs/contracts/DATA_SEMANTICS.md §10）；上游输入
  DATA-P1-CAL（§9）。
- API: API-COS-001（docs/contracts/PUBLIC_API.md，ac_correct_frame/
  ac_correct_frame_f64/ac_set_num_threads）；API-P1-002
  （docs/api/PHASE1_API_V1.md §2，编排合同，多模块共享）。
- MOD/SRC: MOD-astrocs-phase1-cosmetic（lib/algorithms/cosmetic/module.yaml，
  CONTRACT_READY；lib/algorithms/cosmetic/README.md 实现事实）。
- TEST: TEST-COS-DESIGN-001（本文档 §9）；可执行 TEST-P1-COS-001 由
  P1-COS-TEST 建立。
- 摘要引用: ALG-CAL-004（docs/algorithms/CALIBRATION_ALGORITHMS.md §3.4，
  P1-CAL 合同视角同一实现）。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- **宇宙线剔除（单帧）**：van Dokkum 2001, PASP **113, 1420**（LA Cosmic，DOI 10.1086/323894，卷页已核）；Pych 2004, **PASP 116, 148–153**,"A Fast Algorithm for Cosmic-Ray Removal from Single Images"（DOI **10.1086/381786**，卷页与 DOI 经 Crossref + OpenAlex 双源核验）。
  **适用域（两层核验结论）**：这两篇解决的是**单帧宇宙线剔除**（拉普拉斯边缘检测 / 直方图分析），与坏点检测**不是同一问题**：数据源不同（单帧亮场 vs 母版 dark/bias）、统计量不同（边缘响应 / 直方图 vs 全局 median + k·MAD）、目标不同（瞬态事件 vs 固定坏点）。⇒ 二者**不得**被引作本模块坏点检测算法选型的依据，仅作"同类图像缺陷处理"的领域背景。
- **坏点（hot/cold pixel）检测与修复**：本模块为 Project-defined 实现（§1–§5 即其完整规范），算法要素的通用依据见下条"稳健尺度"与"连通域"；**无**外部算法被引为该检测器的选型来源。
- 稳健尺度（median/MAD 换算）：Hoaglin, Mosteller & Tukey (eds.) 1983, Understanding Robust and Exploratory Data Analysis, Wiley（ISBN 0-471-09777-2）。
  **`1.4826·MAD` 的归属**：Rousseeuw & Croux 1993, JASA **88(424), 1273–1283**（DOI 10.1080/01621459.1993.10476408）把 `1.4826·MAD` 当**既有对照基线**引用，其研究对象是 `S_n`/`Q_n` 及其有限样本偏差校正的粗糙近似 ⇒ **该文不是本模块 MAD 有限样本校正的来源**；本模块使用**渐近常数**、不做有限样本校正。若要做，来源为 Akinshin 2022（arXiv:2207.12005 / arXiv:2209.12268）或 Park, Kim & Wang 2020（DOI 10.1080/03610918.2019.1699114）。
- 连通域结构过滤（8 邻接）：二值图像连通分量标准算法（见 Rosenfeld & Kak 1982, Digital Picture Processing）；本模块 Project-defined 实现。
- 插值修复（中值/双线性）：教科书级（Press et al. 2007, Numerical Recipes 3rd ed.）。**差异**：本模块是坏点局部修复，不是通用的图像插值库。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

