# Phase3 Projection/WCS 实现级算法合同（ALG-P3-PROJ-IMPL-001）

> ID: ALG-P3-PROJ-IMPL-001  状态: CONTRACT_READY（P3-PROJ-DOC 冻结，
> 2026-09-11，SA-P3-P25）  模块: astrocs.p3.projection（迁移合同值；
> registry descriptor 占位 astrocs.phase3.wcs 由 P3-PROJ-INT 对齐）
> 上游 SCI: SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN
> V5 SCI-007 2026-08-28，零改动）；承接既有 ALG-P3-002 本域子面
> （G1/G2 施工规格，docs/algorithms/PHASE3_RESAMPLE.md，公式零改动）。
> 本文档为 WCS/投影域**实现级合同**：逐符号源码行号锚定 + 冻结公式 +
> 错误语义 + 并发/确定性合同 + TEST 设计冻结 + 实测偏差登记。
> 生产源: lib/phase3_session/p3_wcs.h（50 行，唯一权威签名头）+
> lib/phase3_session/p3_wcs.cpp（165 行），实测 2026-09-11。

## 1 目的与非目标

- 目的: 冻结 Phase3 TAN(gnomonic) 投影域的全部公共消费面——
  descriptor 构造（CRPIX/CRVAL/CD、parity、PA）、像素↔天球正反映射、
  FITS 关键词文本输出、极点/半球/参数守卫——作为 P3-PROJ-IMPL/TEST/
  INT 的合同基线。
- 非目标: 不实现 SIN/ZEA/CAR/AIT（矩阵 notes 列为显式扩展清单，当前
  仅 TAN，扩展须独立测试+新 claim，SCI-P3 §9a-3）；不做重采样
  （ALG-P3-001/003，phase3_resample2 域）；不做 FITS 文件读写
  （ALG-P3-FITS-IMPL-001 域）；不做会话编排/请求解析（p3_session 域）；
  不修改 SCI 公式（SCI-P3 FROZEN 零改动，§14）。

## 2 身份与落位

- module_id=astrocs.p3.projection（MODULE_MIGRATION_MATRIX P3-PROJ 行
  权威值）；registry 行 MOD-astrocs-phase3-wcs；
  dll_target=astrocs_p3_projection.dll（合同值，尚未存在，由
  P3-PROJ-IMPL 建立，禁止声明 IMPLEMENTED）；现状构建=
  astrocs_phase3_session 静态库成员（根 CMakeLists.txt:460-465，
  p3_wcs.cpp 为五源文件之一）。
- 合同落位: lib/phase3_proj/ 三件套（README r1 + module.yaml
  CONTRACT_READY entrypoint=MISSING + memory.md，迁移目标目录按
  lib/phase2_upm→phase2_samp→phase2_rej→phase2_int→hips_p2→
  phase3_fits 先例新建；lib/phase3_session/ 为会话编排域共享源，
  不整目录归属）。
- 合同链: SCI-P3-001（共享 FROZEN）→ ALG-P3-PROJ-IMPL-001（本文档，
  兼承接 ALG-P3-002 本域子面）→ DATA-P3-WCS（DATA_SEMANTICS §28）+
  API-P3-PROJ-001（PUBLIC_API.md Phase3 投影公共消费面节）→
  TEST-P3-WCS-001（登记面=TEST-P3-WCS-DESIGN-001 设计冻结 VERIFIED，
  §12；可执行面升级归 P3-PROJ-TEST）；编排面 API-P3-001（p3_session
  五段 FROZEN）镜像不变。

## 3 生产源图（实测，2026-09-11）

| 文件 | 行数 | 角色 |
|---|---|---|
| lib/phase3_session/p3_wcs.h | 50 | 唯一权威签名头（P3WcsDescriptor/P3WcsStatus/四函数） |
| lib/phase3_session/p3_wcs.cpp | 165 | 实现（常量+守卫/G1 构造/正反映射/关键词） |
| lib/phase3_session/p3_session.cpp | 329 | 会话消费点（:17/:160/:163/:232/:247-253） |
| tests/backend/p3_wcs_main.cpp | — | 探针（make/p2w/w2p/kw 四模式，printf 协议） |
| tests/backend/test_p1002_gaps.py | — | 独立解析解回归（内联编译链接 p3_wcs.cpp） |
| tests/unit/p3_wcs_test.cpp | 90 | 单元测试（WCS 完整性/尺寸溢出检查） |
| 根 CMakeLists.txt:460-465 | — | 构建挂载（astrocs_phase3_session STATIC） |

- 头部声明锚: p3_wcs.h:11-20（P3WcsDescriptor）/:22-27（P3WcsStatus）
  /:31-34（p3_wcs_make）/:38-39（p3_wcs_pix2world）/:42-43
  （p3_wcs_world2pix）/:46（p3_wcs_fits_keywords）。
- 实现锚: p3_wcs.cpp:13-22（常量）/:24-27（normalize_ra）/:30-90
  （p3_wcs_make）/:93-118（pix2world）/:120-143（world2pix）/:145-163
  （fits_keywords）。
- 命名空间 astrocs::phase3（p3_wcs.cpp:10）；文件头注 :1-2（数学来源
  Calabretta & Greisen (2002) 标准球面三角公式，RA wrap 经 atan2+fmod
  归一）。

## 4 符号冻结（实测签名，不改码）

```cpp
// p3_wcs.h:11-20
struct P3WcsDescriptor {
  double crval_ra_deg;    // 中心 RA (deg, ICRS)
  double crval_dec_deg;   // 中心 Dec (deg, ICRS)
  double crpix_x;         // FITS 1-based pixel-center
  double crpix_y;         // FITS 1-based pixel-center
  double cd[2][2];        // FITS 顺序 CD[i][j], 单位 deg/px, CD-only
  int width_px;           // 输出宽 (px)
  int height_px;          // 输出高 (px)
  const char* projection; // 冻结 "TAN"
};
// p3_wcs.h:22-27
enum class P3WcsStatus {
  P3_WCS_OK = 0,
  P3_WCS_PARAM = 1,        // 参数非法(abs(dec)>85°/W,H 越界/极点守卫)
  P3_WCS_UNSUPPORTED = 2,  // projection≠TAN
  P3_WCS_HEMISPHERE = 3    // 输出跨 TAN 半球
};
// 请求层 projection/frame/coverage_output 合法性 (B2-A4/A5 唯一机器源;
// CLI 配置面 runtime_client::phase_config 与节点面 module_adapters 共用)
P3WcsStatus p3_wcs_validate_request(const char* projection, const char* frame,
                                    const char* coverage_output, std::string* why);
                                                            // p3_wcs.h:37-38
P3WcsStatus p3_wcs_make(double centre_ra_deg, double centre_dec_deg,
                        double scale_deg_per_px, int width_px, int height_px,
                        const char* parity, double rotation_pa_deg,
                        P3WcsDescriptor* out, const char* projection = "TAN");
                                                            // p3_wcs.h:44-47
P3WcsStatus p3_wcs_pix2world(const P3WcsDescriptor* d, double x, double y,
                             double* ra_deg, double* dec_deg);  // h:52-53
P3WcsStatus p3_wcs_world2pix(const P3WcsDescriptor* d, double ra_deg, double dec_deg,
                             double* x, double* y);             // h:56-57
std::string p3_wcs_fits_keywords(const P3WcsDescriptor* d);     // h:60
```

- **投影拒绝（B2-A4 冻结，不得放宽）**：`p3_wcs_validate_request` 对
  projection 仅接受 "TAN"（缺省即 TAN）；SIN/CAR/AIT 即便在
  `lib/phase3_proj` registry 注册（§18.1 首批四投影 claim）也未接入
  alpha 生产路径，故与任意未注册码一样返回 P3_WCS_UNSUPPORTED——
  **禁止静默改写为 TAN**（01_SCIENCE_AUTHORITY_BASELINE §4）。frame
  非 icrs（接受 "ICRS"）→ P3_WCS_UNSUPPORTED；coverage_output 非 mask
  → P3_WCS_PARAM。`p3_wcs_make` 的 projection 默认实参保持既有调用
  零改动，入口先于一切数值构造校验；`p3_wcs_fits_keywords` 的 CTYPE
  由 descriptor 的 projection 派生（TAN 字节不变），未实现投影返回空串。

- 语义冻结: parity 接受 "east_left"（默认，nullptr 归一为
  east_left，p3_wcs.cpp:37，⇒CD1_1<0）与 "east_right"（⇒CD1_1>0），
  其它值 P3_WCS_PARAM（:39）；pix2world/world2pix 入参 x,y 为
  **0-based**（FITS 1-based=+1，p3_wcs.cpp:130-131 内部换算 :140-141
  输出回 0-based）；crpix=(W+1)/2、(H+1)/2（:47-48，FITS 1-based
  pixel-center，与 G1 冻结式一致）。

## 5 SCI/ALG 映射声明

- SCI-P3-WCS-001（descriptor 占位 sci_id）⇒ **SCI-P3-001**（共享
  FROZEN，docs/science/PHASE3_HIPS_TO_FITS.md，V5 SCI-007）：
  G1↔§9a-4（FITS 1-based/CRPIX/CD-only/parity 方向）、G2↔§5 反向
  映射连续定义、TAN-only↔§9a-3、极点拒/半球↔§4+§9a-6、roundtrip
  容差↔§7 不变量（<1e-6 px FP64）与 §9a-12、FOV≤20°/中心距极点
  ≥5°↔§9a-12；descriptor 占位 ID 不入合同，由 P3-PROJ-INT 对齐。
- ALG-P3-002（docs/algorithms/PHASE3_RESAMPLE.md §2 施工规格，
  DERIVED V5 ALG-007）: 本域子面=**G1（输出 WCS 构造）+G2（反向
  映射）**，实现承接于 p3_wcs.cpp（§6/§7）；G3/G4/G5 属重采样/写出
  域（ALG-P3-003/004→ALG-P3-FITS-IMPL-001），非本合同。既有
  ALG-P3-002 公式零改动，本文档为实现级细化（含 P0 修复陈述 §6.3）。
- API-P3-001（会话五段编排 FROZEN）为镜像合同不变：WCS 消费点在
  run 段（p3_session.cpp:160 make/:232 逐像素 pix2world）。

## 6 G1 输出 WCS 构造冻结（p3_wcs_make，p3_wcs.cpp:30-123）

### 6.1 参数校验序（冻结）

`out` 非空（:34）→ parity∈{east_left,east_right}（:39）→
|centre_dec_deg|≤85.0°（:40，kMaxAbsDec :15，SCI/API/session 单一
条件）→ scale_deg_per_px>0（:41）→ W,H∈[1,kMaxSide]（:42-43，
kMaxSide=20000 默认，可 ASTROCS_P3_MAX_SIDE 编译期覆盖 :18-22，
如实冻结）。顺序即实现序；任一失败返回 P3_WCS_PARAM，out 已被
零初始化（:35）。

### 6.2 CD 构造（G1 冻结式，:51-78）

```
CRPIX1=(W+1)/2, CRPIX2=(H+1)/2, CRVAL=center            # :45-48
PA=0 精确形式（G1 对角）:
  east_left:  CD = diag(−s, +s)      # x 增 → RA 减, 北朝上
  east_right: CD = diag(+s, −s)      # x 增 → RA 增, y 增 → Dec 减
PA≠0 一致复合推广（:56-64 展开式）:
  CD = R(−PA)·diag(sgn_x·s, sgn_y·s),  R(−PA)=[[cosPA, sinPA], [−sinPA, cosPA]]
  (sgn_x, sgn_y): east_left=(−1,+1), east_right=(+1,−1)
  CD1_1 = sgn_x·s·cosPA;  CD1_2 = sgn_y·s·sinPA
  CD2_1 = −sgn_x·s·sinPA; CD2_2 = sgn_y·s·cosPA      # :75-78 实现行
```

- 手性约束（:59-61 注释冻结）: TAN 切平面中间坐标 (ξ,η) 沿
  (东,北) 为右手系（det>0）；两种 parity 均要求 det(CD)=
  sgn_x·sgn_y·s²=−s²<0（平面映像镜像一次，保持天球手性），与
  SCI-P3-001 §9a-4 收紧 CD1_1 符号后的 G1 一致。
- PA 语义（:67-68 冻结）: 天北相对 +y 的位置角，逆时针为正；
  PA=0 时 cos=1/sin=0 精确退化到 G1 对角形式；east_left 分支与
  旧实现逐元素 bitwise 一致。

### 6.3 P0 修复陈述（bughunt_p0_wcs，:65-66 注释冻结）

旧实现 east_right 分支误用 sgn_y=+1，使 PA=0 时 CD=diag(+s,+s)，
违反 G1 冻结的 diag(+s,−s)（y 镜像错误）；修复后 sgn_y=−sgn_x
（:71），east_right PA=0 ⇒ CD=diag(+s,−s)。此为已合并的生产修复
事实，本文档如实登记；不再作为待整改项。

### 6.4 四角同半球守卫（:80-88 冻结）

构造完成后对四角像素 (0,0)、(W−1,0)、(0,H−1)、(W−1,H−1)
（0-based）逐一调用 p3_wcs_pix2world，任一返回非 P3_WCS_OK
（即任一角落在 TAN 半球外）⇒ 整体返回该状态（首次失败码透传），
不产出半成品 descriptor。

### 6.5 projection 字段

硬编码 "TAN"（:36 局部 proj，:89 `(void)proj`；out->projection 于
h:19 冻结为 "TAN"）；P3_WCS_UNSUPPORTED=2 枚举现无产生点（备而
不用，SIN/ZEA/CAR/AIT 扩展 TODO，§11）。

## 7 G2 正反映射冻结

### 7.1 p3_wcs_pix2world（像素→天球，p3_wcs.cpp:126-150）

```
空指针守卫: !d || !ra_deg || !dec_deg → P3_WCS_PARAM        # :95
中间坐标 (deg): dx=(x+1)−CRPIX_x, dy=(y+1)−CRPIX_y          # :97-98
              ξ = (CD[0][0]·dx + CD[0][1]·dy)·kRad          # :99
              η = (CD[1][0]·dx + CD[1][1]·dy)·kRad          # :100
TAN 半球守卫: r=√(ξ²+η²) ≥ π/2 → P3_WCS_HEMISPHERE          # :103-104
gnomonic 反投影: θ=atan2(1, r)（=atan(1/r)）                 # :105
              φ=atan2(−ξ, η)（自 +dec 轴向 −RA）            # :106
              Dec = asin(sinθ·sinδ₀ + cosθ·cosδ₀·cosφ)      # :110
              Δα = atan2(−cosθ·sinφ, cosδ₀·sinθ − sinδ₀·cosθ·cosφ)  # :112
                   （Calabretta & Greisen (2002) 论文 II 形式，:111 注释锚）
              RA = normalize_ra(CRVAL_α + Δα)               # :113-114, :24-27
                  （fmod 360 + 负值 +360 → [0,360)）
```

### 7.2 p3_wcs_world2pix（天球→像素，p3_wcs.cpp:152-175）

```
空指针守卫 → P3_WCS_PARAM                                    # :122
极点邻域拒: |dec| > 85° → P3_WCS_PARAM                       # :123
背面守卫: denom = sinδ₀·sinδ + cosδ₀·cosδ·cos(α−α₀) ≤ 0
          → P3_WCS_HEMISPHERE                                # :128-130
gnomonic 投影: ξ = cosδ·sin(α−α₀)/denom                      # :131
              η = (sinδ·cosδ₀ − cosδ·sinδ₀·cos(α−α₀))/denom # :132-133
线性解 CD·δp = (ξ,η) (deg): det=CD11·CD22−CD12·CD21           # :136
  |det|<1e-300 → P3_WCS_PARAM（奇异 CD 拒）                  # :137
  δx=(CD22·ξ−CD12·η)/det, δy=(−CD21·ξ+CD11·η)/det            # :138-139
x = δx + CRPIX_x − 1, y = δy + CRPIX_y − 1（0-based 输出）    # :140-141
```

### 7.3 往返容差（冻结）

`pixel→world→pixel` 误差 **<1e-6 px**（FP64）——SCI-P3-001 §7
独立不变量 + §9a-12 冻结；解析oracle回归现状由
tests/backend/test_p1002_gaps.py 承载（独立解析解，非生产代码
复算）；验收级 oracle=WCSLIB（矩阵 notes），由 P3-PROJ-TEST 建立。

## 8 p3_wcs_fits_keywords 关键词合同（p3_wcs.cpp:177-195）

- 输入 nullptr → 空串（:146）。
- 逐行文本，每行 ≤80 字节（FITS 卡形态），"\n" 分隔；清单冻结：
  `CTYPE1= 'RA---TAN'`、`CTYPE2= 'DEC--TAN'`、`CUNIT1/2= 'deg'`、
  `CRPIX1/CRPIX2`（%.10f）、`CRVAL1/CRVAL2`（%.10f）、
  `CD1_1..CD2_2`（%.12e）——与 G5 关键词面（ALG-P3-FITS-IMPL-001
  §8、p3_output.cpp:157-182）同族；本函数为 descriptor→文本的
  探针/调试面，生产 FITS 头写路径在 p3_output 域（本域不写文件）。
- 数值格式化经 std::snprintf（:147 buf[128]），无缓冲溢出面
  （最长行 "CD1_1  = -1.234567890123e-05 / comment" 量级 <80 字节）。

## 9 错误语义冻结

| 返回码 | 值 | 触发（实现锚） | 会话映射（p3_session.cpp:163） |
|---|---|---|---|
| P3_WCS_OK | 0 | 成功 | — |
| P3_WCS_PARAM | 1 | make: out 空/:39 parity/:40 \|dec\|>85°/:41 scale≤0/:42-43 尺寸越界；pix2world :95 空指针；world2pix :122 空指针/:123 \|dec\|>85°/:137 \|det\|<1e-300 | ACS_ERR_PARAM |
| P3_WCS_UNSUPPORTED | 2 | 无产生点（projection≠TAN 备用枚举，§6.5） | ACS_ERR_UNSUPPORTED |
| P3_WCS_HEMISPHERE | 3 | pix2world :104 r≥π/2；world2pix :130 denom≤0；make 四角守卫透传（:84-87） | ACS_ERR_PARAM |

- 极点/半球单一条件冻结: |dec|≤85°（kMaxAbsDec :15，SCI/API/session
  同一常数）；TAN 半球界 r<π/2 与 denom>0 数学等价（gnomonic 背面
  判定的正反两形态）。
- make 四角守卫透传的返回码可能为 P3_WCS_HEMISPHERE（视场超半球）
  ——即"参数合法但视场越界"仍属失败，不产出 descriptor。

## 10 并发/确定性/资源合同

- 内核纯函数: p3_wcs.cpp grep 实测 0 处 thread/mutex/atomic/omp、
  0 处全局可变状态（匿名命名空间常量+纯函数 :12-28）——const-only
  入口（descriptor 只读）多线程并发安全，descriptor parallel_ok=
  true（module_adapters.cpp:410-427 占位）与此结构性一致。
- RT-006（线程泄漏守卫）由"无内部线程+无全局可变状态"结构性满足；
  depends_on_int 矩阵值 ABI-005;DATA-004;RT-006 之 RT-006 锚本节。
- 并行仅上游采样 worker 池（p3_session.cpp:247-253，worker 数=
  host budget.max_workers，:209 注释禁 hardware_concurrency）；本域
  逐像素 pix2world 在 worker 内串行调用（:232），失败 continue
  （半球外像素保持 NaN/无覆盖语义，DATA-P3-WCS §28.1）。
- 确定性: 纯函数无浮点求和序问题；同入参跨平台/跨线程 bitwise
  一致（libm 超越函数平台差由测试层双平台数值合同覆盖，§12）。
- 资源: 无动态分配（除 fits_keywords std::string）；O(1) 每调用。

## 11 实测偏差与整改登记（不修码，如实冻结）

- **PA 未接线**: p3_session.cpp:160 rotation_pa_deg 实参恒 0.0——
  内核 PA 能力（§6.2 推广 CD）无会话消费方；会话请求 schema 无
  rotation 字段。整改归 P3-PROJ-IMPL（接线）/P3-PROJ-INT（请求词
  汇对齐）。
- **kMaxSide 编译期可覆盖**: ASTROCS_P3_MAX_SIDE（:18-22）——合同
  上限 20000 为默认值语义（PHASE3_API_V1 §2 资源/配置合同，
  :16-17 注释锚），覆盖属构建期显式行为，非静默偏差；如实登记。
- **projection 硬编码**: §6.5——UNSUPPORTED 枚举备而不用；非 TAN
  扩展按 SCI §9a-3 须独立测试+新 claim。
- **astrocs_p3_projection.dll 未建**: entrypoint=MISSING；探针/
  回归现状内联编译（test_p1002_gaps.py / tests/unit/CMakeLists），
  非 DLL 挂载；由 P3-PROJ-IMPL 建立。
- descriptor 占位词汇（module_id=astrocs.phase3.wcs、sci_id=
  SCI-P3-WCS-001、alg_id=ALG-P3-002、test_id=TEST-P3-WCS-001、
  端口 props(DATA-P3-PROPS 必)+wcs_plan(DATA-P3-WCS 可)）为编排层
  词汇（module_adapters.cpp:410-427），由 P3-PROJ-INT 对齐
  astrocs.p3.projection，不得反向作为冻结依据。

## 12 TEST-P3-WCS-DESIGN-001 设计冻结（登记面 VERIFIED）

> 可执行测试现状三处如实登记（引用不冒认，验收级升级归
> P3-PROJ-TEST）: tests/unit/p3_wcs_test.cpp（90 行）、
> tests/backend/test_p1002_gaps.py（独立解析解回归）、
> tests/backend/p3_wcs_main.cpp（探针）。

- **T1 正向解析解**: 已知天球点（含 RA wrap 跨 0/360、|dec|=60°/85°
  边内、四象限 PA∈{0°,90°,−90°,30°}）→ p3_wcs_world2pix → 与
  独立解析解（Calabretta & Greisen 论文 II 形式直接计算）比对，
  容差 <1e-9 px（FP64 机器精度量级）。
- **T2 往返不变量（冻结容差）**: 像素网格全扫描
  p3_wcs_pix2world∘p3_wcs_world2pix 误差 <1e-6 px（SCI §7 冻结值，
  禁放宽）；半球边界附近（r→π/2）除外（HEMISPHERE 拒绝语义）。
- **T3 G1 构造精确断言**: PA=0 ⇒ CD 精确等于 diag(±s,∓s)
  （east_left/east_right 两分支，bitwise 级）；PA=90° ⇒ CD1_2=
  sgn_y·s、CD2_1=−sgn_x·s（§6.2 展开式逐元素）；crpix=(W+1)/2
  奇偶双例（W=512 ⇒ 256.5，W=513 ⇒ 257.0）。
- **T4 手性/极性断言**: east_left PA=0 下 x 增 → RA 减（北极朝上
  图像）；det(CD)=−s² 恒成立；p3_wcs_fits_keywords 输出含
  CTYPE='RA---TAN'/'DEC--TAN' 且每行 ≤80 字节。
- **T5 负面清单**: parity 非法串/nullptr→PARAM；|centre_dec|=85.1°
  →PARAM；scale≤0→PARAM；W 或 H=0/20001→PARAM；world2pix
  |dec|=85.1°→PARAM；极点（dec=±90°）→PARAM；背面天球点
  （与 CRVAL 角距>90°）→HEMISPHERE；视场超半球（大 FOV 四角）
  →make 返回 HEMISPHERE。
- **T6 oracle**: 现状=独立解析解（test_p1002_gaps.py）；验收级=
  **WCSLIB 独立实现**（矩阵 notes "WCSLIB test oracle"，由
  P3-PROJ-TEST 建立，禁止用生产实现自证）；双平台数值合同按
  backend 数值测试族先例。
- **T7 不变量/回归**: tests/unit/p3_wcs_test.cpp 溢出检查与 WCS
  完整性面保持通过；RAFT: 同入参 1/N worker 结果 bitwise 一致
  （结构性满足，纯函数）。

## 13 合同边界与 DISP 登记

- 本域无 DISP 缺陷登记（现状无已知缺陷；P0 修复已合并，§6.3 如实
  陈述不再登记）。
- 整改项（非缺陷，§11）: PA 接线、dll/入口建立、WCSLIB oracle
  建库——均归 P3-PROJ-IMPL/TEST/INT，本合同层不修码。
- 边界: 不改 vendored 第三方；不改 SCI 公式（§14）；跨域消费
  （p3_session/p3_output）只登记不修；模块页=
  docs/modules/phase3_proj.md + registry 手写页
  docs/modules/registry/astrocs.phase3.wcs.md。

## 14 SCI 层零改动声明

- docs/science/PHASE3_HIPS_TO_FITS.md（SCI-P3-001，FROZEN V5
  SCI-007 2026-08-28）本任务零改动；G1/G2/TAN-only/容差全部以
  §9a/§5/§7 冻结值为唯一推导来源；实现与 SCI 的任何不一致按
  纪律登记 DISP/偏差（§11），禁止反向修改 SCI。

## 15 P3-001 增补：版本化 projection registry 与冻结四投影（TAN/SIN/CAR/AIT）

> 增补 2026-09-10，任务 P3-001（ASTROCS-CONSTITUTION-ALIGNMENT-V1，
> BASE=9e0fa3a8）。宪章 ASTROCS-CONSTITUTION-001 §7.3（"投影由版本化
> projection registry 注册，投影实现不能散落在 CLI switch 中；每种投影
> 必须声明适用天区、奇点、经纬方向、CRPIX/CRVAL/CD/CTYPE 规则、合法 FOV
> 和独立往返 Oracle"）与 §18.1 负责人裁决 1（首批投影冻结为
> `TAN + SIN + CAR + AIT`；新增投影必须经 projection registry 注册并附
> 独立往返 Oracle）。TAN 冻结零改动（§6/§7 沿用）；SIN/CAR/AIT 为 §18.1
> 裁决新增 claim，公式 = Calabretta & Greisen (2002) FITS WCS Paper II
> 标准定义；本节即该 claim 的 ALG 层唯一权威落位。SCI 层零改动（§14
> 不变：TAN-only 拒绝面为 SCI alpha 会话合同，四投影 registry 为宪章
> §18.1 上位裁决的独立实现面，两者由会话层合同衔接，不互改）。

### 15.1 registry 冻结表（v1）

- 版本常量 `kP3ProjectionRegistryVersion = 1`；表内容或语义变化必须递增
  版本并在本节登记变更 claim。表恰 4 行、顺序冻结：

| 行 | id | code | CTYPE1/CTYPE2 | 中心守卫 | 合法 FOV 声明 | 域 |
|---|---|---|---|---|---|---|
| 0 | TAN | "TAN" | RA---TAN / DEC--TAN | \|dec\|≤85° | 20°（SCI §9a-12 冻结） | zenithal, θ₀=CRVAL2 |
| 1 | SIN | "SIN" | RA---SIN / DEC--SIN | \|dec\|≤85° | 60°（claim） | zenithal, θ₀=CRVAL2 |
| 2 | CAR | "CAR" | RA---CAR / DEC--CAR | \|dec\|≤85° | 180°（claim） | cylindrical, θ₀=+90° |
| 3 | AIT | "AIT" | RA---AIT / DEC--AIT | \|dec\|≤85° | 360°（claim） | pseudo-cylindrical, θ₀=+90° |

- 落位：`lib/phase3_proj/p3_projection.h`（唯一权威签名头）+
  `lib/phase3_proj/p3_projection.cpp`（实现）。查询 API：`registry_table`
  （表首指针+行数）/`registry_find(code)`（精确匹配，未注册 → nullptr，
  无 fallback 无静默）/`registry_find_id`（越界 → nullptr）/
  `registry_selfcheck`（行数/码互异/CTYPE 非空/函数指针非空/id 顺序）。
- dispatch 经 spec 内函数指针（`pix2world`/`world2pix`），实现不散落
  switch（宪章 §7.3）；projection 字段为 `P3ProjectionId` 强类型枚举，
  杜绝 CLI 字符串散落。
- max_fov_deg 为 registry **声明字段**（宪章 §7.3 "合法 FOV"），非 make
  硬门——FOV 裁决属会话层合同（TAN alpha 的 FOV≤20° 强制点在 SCI §4/
  §9a-12 会话合同），投影域本身由四角守卫 + 投影域界（§15.3）承载。

### 15.2 统一管线与共享核（rad 内部计算）

- 正向（world→pixel）: (α,δ) → 旋转核逆 → native (φ,θ) → 投影层 →
  中间坐标 (X,Y) deg → CD⁻¹（解析 2×2，|det|<1e-300 → PARAM）→
  0-based 像素。逆向（pixel→world）: 像素 → CD·δp → (X,Y) → 投影层逆 →
  (φ,θ) → 旋转核 → (α,δ)，RA 经 fmod 归一 [0,360)。
- zenithal 旋转核（TAN/SIN，θ₀=CRVAL2=δ₀；表达式顺序与 §6/§7 冻结式
  一致）:
  `sinθ = sinδ sinδ₀ + cosδ cosδ₀ cosΔα`（=denom，≤0 → HEMISPHERE）
  `φ = atan2(−cosδ sinΔα, sinδ cosδ₀ − cosδ sinδ₀ cosΔα)`
  `δ = asin(sinθ sinδ₀ + cosθ cosδ₀ cosφ)`
  `Δα = atan2(−cosθ sinφ, cosδ₀ sinθ − sinδ₀ cosθ cosφ)`
- CAR/AIT 天球惯例：θ₀=+90°（native 北极=天球北极，LONPOLE=0 语义）⇒
  native (φ,θ)=(α−α₀, δ) 恒等旋转；**CRVAL2 仅记录于 header 不进入映射**
  （CRVAL1=中央经线 α₀ 参与映射）。此为四投影统一 descriptor 语义下的
  显式冻结声明（FITS celestial CAR/AIT 实践一致）。
- RA 最短角差（CAR/AIT）: dra=fmod(α−α₀,360) 归一 (−180,180]（−180 归
  +180，唯一化）。

### 15.3 四投影投影层（逐式冻结）

- **TAN**（§6/§7 冻结零改动，冻结逐式路径）: R=cotθ；
  X=−R sinφ, Y=R cosφ。逆: r=√(X²+Y²) (rad)，r≥π/2 → HEMISPHERE；
  θ=atan2(1,r)，φ=atan2(−X,Y)。与 lib/phase3_session/p3_wcs.cpp 生产实现
  bitwise 一致（§15.6 T3 对拍承载）。
- **SIN**（orthographic）: R=cosθ；X=−R sinφ, Y=R cosφ。逆:
  ρ=√(X²+Y²) (rad)，ρ>1 → HEMISPHERE（ρ=1 边界合法，θ=0）；
  θ=acos(ρ)，φ=atan2(−X,Y)。奇点声明：半球边界圆 ρ=1；中心守卫
  |CRVAL dec|≤85° 为保守收窄（SIN 极视场数学可行，冻结域不含）。
- **CAR**（plate carrée）: X=φ, Y=−θ。逆: φ=X·kRad, θ=−Y·kRad，
  |θ|>90° → PARAM（冻结域 |δ|≤90° fail-closed，拒绝柱面延伸域另一支）。
  无投影奇点（δ 线性）；高纬面积畸变由 FOV 声明承载。
- **AIT**（Aitoff，θ₀=+90°）: 正向 D=√(1+cosθ·cos(φ/2))；
  X=2cosθ·sin(φ/2)/D, Y=sinθ/D。逆: A=X²/4+Y²（rad²），
  D²=2−A，D²≤0 → HEMISPHERE（椭圆域）；sinθ=Y·D，|sinθ|>1 →
  HEMISPHERE；θ=asin(sinθ)，φ=2·atan2(X·D/2, D²−1)（D²=1 即 φ=±180°
  边界合法，atan2 唯一）。反演式由恒等式 A=1−cosθ·cos(φ/2) 封闭推导
  （X²/4+Y²=1−v, v=cosθ·cos(φ/2)），等价 Paper II 反演。奇点声明：
  椭圆域边界 X²/4+Y²=2；全天空（360°×180°）为设计目标域。

### 15.4 descriptor/make 守卫（四投影统一）

- make 参数校验序与 TAN 冻结序一致（§6.1）: out 非空 → id 注册校验
  （越界 → UNSUPPORTED）→ parity∈{east_left,east_right}（nullptr 归一
  east_left）→ |centre_dec_deg|≤85°（四投影统一保守冻结，TAN 侧=SCI
  单一条件）→ scale>0 → W,H∈[1,20000]（kMaxSide 默认，可
  ASTROCS_P3_MAX_SIDE 编译期覆盖）→ G1 CD 构造（§6.2 逐式，四投影
  同构）→ 四角投影域守卫（0-based (0,0)/(W−1,0)/(0,H−1)/(W−1,H−1)
  逐一调投影域检查，任一失败 → 首败码透传，不产半成品 descriptor）。
- 状态码（P3ProjectionStatus，值域与 P3WcsStatus 冻结对齐）: OK=0 /
  PARAM=1 / UNSUPPORTED=2（未知码/越界 id） / HEMISPHERE=3。
- 域界语义冻结: TAN r≥π/2 / SIN ρ>1 / AIT D²≤0 → HEMISPHERE；
  CAR/AIT 天球端 |δ|>90°、CAR 平面端 |θ|>90° → PARAM；TAN/SIN
  world2pix 端 |dec|>85° → PARAM（§9 冻结语义沿用）。
- fits_keywords: CTYPE1/2 经 registry spec 解析（禁硬编码 CTYPE 于
  调用方），其余行与 §8 同族（CRPIX/CRVAL %.10f、CD %.12e、每行
  ≤80 字节、12 行）；descriptor 空指针/未注册 id → 空串（fail-closed
  不产 CTYPE 面）。

### 15.5 六要素声明（宪章 §7.3 逐投影）

| 投影 | 适用天区 | 奇点 | 经纬方向 | CRPIX/CRVAL/CD/CTYPE 规则 | 合法 FOV | 独立往返 Oracle |
|---|---|---|---|---|---|---|
| TAN | \|CRVAL dec\|≤85°, 视场同半球 | 天顶反面 r≥π/2 | parity 显式（§9a-4） | §6/§7 冻结；CRVAL=切点 | ≤20°（SCI 冻结） | 3D 向量 gnomonic 透视重建（§15.6 T2/T4） |
| SIN | \|CRVAL dec\|≤85°, 半球内 | 半球边界 ρ=1 | parity 显式 | 同 G1；CRVAL=投影点 | ≤60°（claim） | 3D 向量 orthographic 重建 |
| CAR | \|CRVAL dec\|≤85°（保守收窄）, \|δ\|≤90° | 无（δ 线性） | parity 显式；CRVAL1=中央经线 | 同 G1；CRVAL2 不进映射 | ≤180°（claim） | 恒等旋转独立式 |
| AIT | \|CRVAL dec\|≤85°（保守收窄）, \|δ\|≤90° | 椭圆域边界 | parity 显式；CRVAL1=中央经线 | 同 G1；CRVAL2 不进映射 | ≤360°（claim，全天空设计域） | Paper II 反演独立式 + 3D 重建 |

- "保守收窄"：SIN/CAR/AIT 中心守卫统一沿用 85° 单一条件（与 TAN 同值），
  属冻结域收窄（alpha 原则），极点中心视场排除；放宽须经宪章变更流程。
- parity/PA 语义四投影统一（§6.2 冻结式原样）：CD 构造与投影无关
  （G1 对角 + PA 推广 + P0 修复内含）。

### 15.6 TEST-P3-PROJ-REG-001（登记面 + 可执行面同文件承载）

- 可执行面两处（本任务建立）:
  - `tests/unit/p3_projection_test.cpp`（ctest: p3_projection_units /
    p3_projection_fault）:
    T1 registry 完整性（版本常量=1/恰 4 行/码与 id 顺序/函数指针/
    selfcheck=0/未知码 nullptr 无 fallback）；T2 每投影独立往返 Oracle
    （3D 单位向量第一性原理：切基正交投影+TAN 透视除法+SIN 正交重建+
    CAR/AIT 恒等旋转独立式+Paper II 反演式；往返 <1e-6 px 冻结）；
    T3 TAN 与 lib/phase3_session/p3_wcs.cpp 生产实现全网格 bitwise
    对拍（CD/CRPIX/正反映射零漂移证明）；T4 world2pix 正向独立解析解
    （3D 向量分量→CD⁻¹，全四投影）；T5 G1 CD 精确断言（PA=0 对角
    bitwise/PA=90° 展开式/det<0/crpix 奇偶双例，四投影同构）；
    T6 负面清单（未知码/parity 非法/|dec|=85.1/scale≤0/W=0/H=20001/
    空指针全族/SIN 视场超半球/AIT 域外/SIN 背面点/CAR |dec|>90 全部
    显式拒绝）；T7 CTYPE 关键词面（RA---<code>/DEC--<code>、12 行、
    ≤80 字节）；T8 确定性（重复计算 bitwise）；T9 1/N worker（1 vs
    2/4/8 线程分块 bitwise，纯函数并发安全实测）。
  - `tests/backend/test_p3_projection_oracle.py`（pytest）:
    Python/numpy 侧**完全独立**第一性实现（与 C++ 测试代码路径亦不同）
    对拍生产 registry（driver 内联编译生产源）；解析比对 <1e-9 deg、
    往返 <1e-6 px、跨进程两次运行 stdout sha256 一致、CTYPE 面。
- 故障注入（必败面，P2-002 先例同构，测试级注入、生产源零 getenv）:
  `ASTROCS_P3PROJ_FAULT=tan|sin|car|ait|registry` 注入等价缺陷（往返
  ±1 px 偏移期望 / registry 静默 fallback），注入模式断言必败并报告
  捕获（FAULT-EFFECT-CONFIRMED），证明测试对对应缺陷类有区分力。
- 验收级 oracle 升级（WCSLIB 独立实现，§12 T6）仍归 P3-PROJ-TEST，
  本任务不冒认。

### 15.7 构建挂载与越界登记

- 测试挂载仅动 `tests/unit/CMakeLists.txt`（add_executable 直编
  p3_projection.cpp + p3_wcs.cpp 对拍 TU，先例 aio_abi_tests 同构）；
  根 CMakeLists.txt / lib/phase3_session 零改动——生产构建挂载
  （astrocs_p3_projection.dll target/adapter 接线/会话消费）归
  P3-PROJ-IMPL/P3-002（白名单外），本任务 out_of_scope_entries=0。
- registry 现状为**测试目标直编面**：非生产构建成员、非 DLL 入口；
  module.yaml 维持 CONTRACT_READY/entrypoint=MISSING 不冒认
  IMPLEMENTED（升级归挂载任务）。

### 15.8 文献锚（§15 新增 claim 引用定位）

- Calabretta & Greisen 2002, A&A 395, 1077（Paper II）：TAN/SIN/CAR
  Table 1 投影定义（R_θ 形式）；AIT 反演与 θ₀=±90° 天球惯例（§2.1
  旋转、§5）；LONPOLE 语义本实现固定 θ₀=+90°/无 φ_p 附加旋转（§15.2
  显式冻结，不实现通用 LONPOLE 机制）。
- Greisen & Calabretta 2002, A&A 395, 1061（Paper I）：CD-only/CTYPE/
  CRPIX 1-based 约定（既有 §5 锚不变）。
- 3D 向量 oracle 第一性原理（切基正交投影/透视除法）为 Project-defined
  独立推导路径，与生产球面三角公式互为独立验证。
