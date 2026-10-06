# Phase3 Projection/WCS 实现级算法合同（ALG-P3-PROJ-IMPL-001）

> 上游：ACSD_DESIGN.md §6.3（投影算法）

> 上游 SCI: SCI-P3-001（docs/science/PHASE3_HIPS_TO_FITS.md，FROZEN，
> 同步口径见 §14）；承接 ALG-P3-002 本域子面
> （G1/G2 施工规格，docs/science/algorithms/PHASE3_RESAMPLE.md）。
> **本域现行口径**：§15 依据 = `ACSD_DESIGN.md` §6.3 八投影 +
> projection registry（现行集合：CRVAL2 进映射 / AIT A≤1 / CAR 极行 fail-closed）+ 对照口径偏差表（离线对照用）；
> §14 = 以独立证据判定、不预设谁为准。订正原则见 `docs/ACSD_DESIGN.md` §0（文档权威与索引）。
> 本文档为 WCS/投影域**实现级合同**：逐符号源码行号锚定 + 冻结公式 +
> 错误语义 + 并发/确定性合同 + TEST 设计冻结 + 实测偏差登记。
> 生产源: lib/algorithms/projection/p3_wcs.h（373 行，唯一权威签名头）+
> lib/algorithms/projection/p3_wcs.cpp（593 行），已由 lib/phase3_session/ 迁入本目录
> （内容逐字节等价，行数按新址复测）。

## 1 目的与非目标

- 目的: 冻结 Phase3 TAN(gnomonic) 投影域的全部公共消费面——
  descriptor 构造（CRPIX/CRVAL/CD、parity、PA）、像素↔天球正反映射、
  FITS 关键词文本输出、极点/半球/参数守卫——作为 P3-PROJ-IMPL/TEST/
  INT 的合同基线。
- 非目标: **生产 alpha 路径仍只走 TAN**（lib/algorithms/projection/p3_wcs.cpp，
  会话合同 SCI-P3 §9a-3 收窄）；registry 冻结集合按 ACSD_DESIGN §6.3
  八投影（TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA），已实现 4/8、
  STG/MOL/CEA/ZEA 实施归 P3-001（新增投影必须落在冻结集合内并附独立
  Oracle）；本文件不臆造未实现投影的公式（§15.1/§15.3）；不做重采样
  （ALG-P3-001/003，phase3_resample2 域）；不做 FITS 文件读写
  （ALG-P3-FITS-IMPL-001 域）；不做会话编排/请求解析（p3_session 域）；
  不修改 SCI 公式（SCI-P3 FROZEN 零改动，§14）。

## 2 身份与落位

- module_id=acsd.p3.projection（MODULE_MIGRATION_MATRIX P3-PROJ 行
  权威值）；registry 行 MOD-acsd-phase3-wcs；
  dll_target=acsd_p3_projection.dll（合同值，尚未存在，由
  P3-PROJ-IMPL 建立；IMPLEMENTED 词由验收在建立后签发）；现状构建=
  acsd_phase3_session 静态库成员（根 CMakeLists.txt:460-465，
  p3_wcs.cpp 为五源文件之一）。
- 合同落位: lib/algorithms/projection/ 三件套（README r1 + module.yaml
  CONTRACT_READY entrypoint=MISSING + memory.md，迁移目标目录按
  lib/algorithms/upm→phase2_samp→phase2_rej→phase2_int→hips_p2→
  phase3_fits 先例新建；lib/phase3_session/ 为会话编排域共享源，
  不整目录归属）。
- 合同链: SCI-P3-001（共享 FROZEN）→ ALG-P3-PROJ-IMPL-001（本文档，
  兼承接 ALG-P3-002 本域子面）→ DATA-P3-WCS（`docs/detail/registry/acsd.phase3.wcs.md`）+
  API-P3-PROJ-001（PUBLIC_API.md Phase3 投影公共消费面节）→
  TEST-P3-WCS-001（登记面=TEST-P3-WCS-DESIGN-001 设计冻结 VERIFIED，
  §12；可执行面升级归 P3-PROJ-TEST）；编排面 API-P3-001（p3_session
  五段 FROZEN）镜像不变。

## 3 生产源图（实测）

| 文件 | 行数 | 角色 |
|---|---|---|
| lib/algorithms/projection/p3_wcs.h | 373 | 唯一权威签名头（P3WcsDescriptor/P3WcsStatus/四函数 + 适用域/Oracle 面）；已迁入本目录，行数按新址复测 |
| lib/algorithms/projection/p3_wcs.cpp | 593 | 实现（常量+守卫/G1 构造/正反映射/关键词 + 适用域门 `p3_wcs_check_applicability` + 往返门 `p3_wcs_roundtrip_*`）；行数按新址复测 |
| lib/phase3_session/p3_session.cpp | 329 | 会话消费点（:17/:160/:163/:232/:247-253） |
| 根 CMakeLists.txt:460-465 | — | 构建挂载（acsd_phase3_session STATIC） |

> WCS 完整性/尺寸溢出检查、独立解析解回归、四模式探针三组判据的载体文件**均已不在本仓**；
> 判据本体（溢出检查、解析解对拍、printf 四模式协议）继续有效，改由实验复算与人读对抗审核执行。

- 头部声明锚: p3_wcs.h:12-20（P3WcsDescriptor）/:22-27（P3WcsStatus）
  /:31-34（p3_wcs_make）/:38-39（p3_wcs_pix2world）/:42-43
  （p3_wcs_world2pix）/:46（p3_wcs_fits_keywords）。
- 实现锚（**符号名优先**；行号随适用域门/Oracle 面扩写整体下移，§3 表为现行实测）:
  p3_wcs.cpp:13-22（常量）/:24-31（normalize_ra）/:92-171（`p3_wcs_make`，
  含投影校验、G1 CD 构造 :120-143、四角守卫 :146-155、适用域门 :156-166）
  /:515-540（`p3_wcs_pix2world`）/:542-568（`p3_wcs_world2pix`）
  /:570-593（`p3_wcs_fits_keywords`）；适用域与往返门
  `p3_wcs_applicability` / `p3_wcs_check_applicability` / `p3_wcs_roundtrip_*`
  在同文件内，为容差唯一事实源。
- 命名空间 acsd::phase3（p3_wcs.cpp:10）；文件头注 :1-2（数学来源
  Calabretta & Greisen (2002) 标准球面三角公式，RA wrap 经 atan2+fmod
  归一）。

## 4 符号冻结（实测签名，不改码）

```cpp
// p3_wcs.h:12-20
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
// 请求层 projection/frame/coverage_output 合法性 (唯一机器源;
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

- **投影拒绝（冻结，取值一律按本节）**：请求面**唯一语义源** =
  `lib/algorithms/projection/p3_projection_registry.h`（自述 = 「DESIGN §6.3 的
  唯一产品声明权威」）。三层集合（该头 :14-20 冻结，**唯一权威**）：
  ① 冻结集 **F** = 八投影 `TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA`
  （`p3_proj_frozen_table` :66-89）；
  ② 实现集 **I** = 生产路径真正有内核的码（`p3_proj_is_implemented` :145-150，
  `kImplemented[1] = {"TAN"}` **:140-143**）；
  ③ 声明集 **D** = 可作为产品声明的码（`p3_proj_is_declared` :127-131，
  `kDeclared[1] = {"TAN"}` **:122**）。
  判据 **D == I == 实际可运行集 R**，任一不等即判红（:19-20；自检
  `p3_proj_registry_selfcheck` **:272**（D ⊆ I 判据 **:295**））。
  ⇒ **产品面受理集 = {TAN}**。`p3_wcs_validate_request` :68 经
  `p3_proj_declare`（:165-191）判定，**非声明码一律 :69 返回
  P3_WCS_UNSUPPORTED**（SIN/CAR/AIT/STG/MOL/CEA/ZEA 与任意未知码同路）；
  `p3_wcs_make` 另设两道保险：canonical 码查不到（:99）与
  `!p3_proj_is_implemented`（:100）。

  **逐码产品状态（注册表 :69-89，三值无第四态）**：

  | 码 | 状态 | 依据 |
  |---|---|---|
  | TAN | `kProductDeclarable` | 生产内核 `p3_wcs.cpp` + 适用域已声明 |
  | SIN / CAR / AIT | **`kKernelOnly`** | v6 内核（`p3_proj.h/.cpp`，`kProjectionRegistryVersion=3`）已实现 4/8，**但未接入生产路径 ⇒ 不是产品声明** |
  | STG / MOL / CEA / ZEA | `kNotImplemented` | 无内核、无生产接线 |

  **内核 4/8 是内部事实，不得表述为产品能力**（注册表头 :10-12 冻结）。
  frame 非 icrs/ICRS（:72-74）→ P3_WCS_UNSUPPORTED；coverage_output 非 mask
  （:77-81）→ P3_WCS_PARAM。**绝不静默回落 TAN**（:66-67/:91 注释；注册表 :22-23），
  TAN 只来自显式请求或缺省（`(void)proj` 旧写法已废）。入口先于一切数值构造
  校验（§6.1）；`p3_wcs_fits_keywords` 的 CTYPE 由 descriptor 的 projection
  派生，非声明码返回空串。

> **口径勘误（DISP-P3PROJ-002，按 R-51 改写）**：本节第一版把「v6 内核已实现
> 4/8」误读为「产品面受理四投影」，据此写「已实现四投影一律按请求受理」，
> 与注册表头三层模型（**D = I = {TAN}**）矛盾。现按注册表头（唯一权威）恢复
> 三层表述：**产品面受理集 = {TAN}**；内核 4/8 属内部事实，不作产品能力表述。
> 本节更早版本「projection 仅接受 TAN」的**结论正确、依据缺三层模型**，一并
> 按注册表重述。`ACSD_DESIGN.md` §6.3:493「（当前仅 TAN 可用）」与注册表
> **一致**，不改顶层（原上呈的"跨层冲突"经复核**不成立、已撤销**）。

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
  容差↔§7 不变量（<1e-8 px FP64，生产注册表 `p3_wcs.cpp`（`kTanApplicability`，单一事实源 `p3_wcs_applicability()`））与 §9a-12、FOV≤20°/中心距极点
  ≥5°↔§9a-12；descriptor 占位 ID 不入合同，由 P3-PROJ-INT 对齐。
- ALG-P3-002（docs/science/algorithms/PHASE3_RESAMPLE.md §2 施工规格，
  DERIVED V5 ALG-007）: 本域子面=**G1（输出 WCS 构造）+G2（反向
  映射）**，实现承接于 p3_wcs.cpp（§6/§7）；G3/G4/G5 属重采样/写出
  域（ALG-P3-003/004→ALG-P3-FITS-IMPL-001），非本合同。既有
  ALG-P3-002 公式零改动，本文档为实现级细化（含 P0 修复陈述 §6.3）。
- API-P3-001（会话五段编排 FROZEN）为镜像合同不变：WCS 消费点在
  run 段（p3_session.cpp:160 make/:232 逐像素 pix2world）。

## 6 G1 输出 WCS 构造冻结（p3_wcs_make，p3_wcs.cpp:30-123）

### 6.1 参数校验序（冻结）

`out` 非空（:90）→ **投影注册/实现门（`:91-100`，见下）** →
`*out` 零初始化（:101）→ parity∈{east_left,east_right}（:102-104，
nullptr 归一 east_left）→ |centre_dec_deg|≤85.0°（:105，kMaxAbsDec :20，
SCI/API/session 单一条件）→ scale_deg_per_px>0 → W,H∈[1,kMaxSide]
（kMaxSide=20000 默认，可 ACSD_P3_MAX_SIDE 编译期覆盖 :23-27，
如实冻结）。顺序即实现序；任一失败返回 P3_WCS_PARAM（投影门失败返回
P3_WCS_UNSUPPORTED，见下），out 已被零初始化。

**投影门是校验序的第 2 步、先于一切数值构造**（旧版本节漏记此步、
与 §15.4 同文两说，DISP-P3PROJ-001 勘误）：`p3_wcs_validate_request`
（:64-84）经**产品声明门** `p3_proj_declare`（:68）判**非声明码**
（D = {TAN}，见 §5）；`p3_wcs_make` 另设两道保险：(a) `frame` 非 icrs
（:74）、(b) canonical 码查不到（:99）、(c) `!p3_proj_is_implemented`
（:100，**防 D ⊄ I 回归的保险；D = I = {TAN} 下当前不可达**）——
**绝不静默回落 TAN**，也不产半成品 descriptor（:91 注释冻结）。

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
  lib/algorithms/projection/p3_wcs.cpp 生产实现逐元素 bitwise 一致。

### 6.3 east_right 符号约定（:65-66 注释冻结）

east_right 分支取 sgn_y=−sgn_x（:71），PA=0 ⇒ CD=diag(+s,−s)，
与 G1 冻结的 diag(+s,−s) 一致（east_left 与之互为 y 镜像）。
此为现行生产语义，本文档如实登记。

### 6.4 四角域守卫（:146-155 冻结）

构造完成后对四角像素 (0,0)、(W−1,0)、(0,H−1)、(W−1,H−1)
（0-based）逐一调用 p3_wcs_pix2world，任一返回非 P3_WCS_OK
（即任一角落在域守卫之外，§9 已订正判据语义）⇒ 整体返回该状态（首次失败码透传），
不产出半成品 descriptor。

- **充分性（正向约束 + 证明）**：`pix2world` 的域守卫集
  `R = {(x,y) : |CD·((x,y)+1−CRPIX)|·kRad < π/2}` 是仿射映射 `CD` 下**圆盘的原像**
  ⇒ `R` 是**凸集**（PA≠0 时为椭圆盘，仍凸）。矩形帧 `Q` 是四角的凸包，
  故 `四角 ⊆ R ⇒ Q ⊆ R`：**四角检查对矩形帧既充分又必要**。
  实测锚（同一探针 PART3）：角点刚在域内的帧（r_corner=1.5501 < π/2），
  1001×1001 稠密扫描 1002001 点**零拒绝**；角点在域外的帧（r_corner=1.8512）
  被拒点数与判据 `r≥π/2` 的预测**逐点一致**（mismatch=0）。
  ⇒ 该检查的适用域 = `R` 为凸的任意帧（含 PA≠0）；对**非矩形**输出区域（若未来引入）
  必须重新论证凸性。

### 6.5 projection 字段

`out->projection` 取**已校验的 canonical 码**（`p3_wcs.cpp:1-58` 同族；
声明表 = `p3_projection_registry.h`）。

**P3_WCS_UNSUPPORTED=2 是活跃的 fail-closed 出口、不是备用枚举**
（旧版本节称"现无产生点"为误，DISP-P3PROJ-001 勘误）。实测产生点：

| # | 产生点（p3_wcs.cpp） | 触发条件 |
|---|---|---|
| 1 | **:69**（主入口，`p3_wcs_make` 内 :95 透传） | 请求码**非产品声明集 D = {TAN}**：SIN/CAR/AIT（`kKernelOnly`）、STG/MOL/CEA/ZEA（`kNotImplemented`）与任意未知码**同路** |
| 2 | :74 | `p3_wcs_validate_request`：`frame` 非 icrs/ICRS |
| 3 | :99 | `p3_wcs_make`：canonical 码查不到（防御） |
| 4 | :100 | `p3_wcs_make`：`!p3_proj_is_implemented`（**防 D ⊄ I 的保险；D = I = {TAN} 下当前不可达**） |
| 5 | :438 | `p3_wcs_applicability` 守卫：投影无声明适用域 |

registry 侧同码透传：`p3_projection.cpp:332/:369/:378`。故**除 TAN 外的
全部冻结码与任意未知码一律走此码（主入口 :69）**；「产品面受理四投影」
的读法不成立（注册表三层模型 D = I = {TAN}，见 §5）。

## 7 G2 正反映射冻结

### 7.1 p3_wcs_pix2world（像素→天球，p3_wcs.cpp:126-150）

```
空指针守卫: !d || !ra_deg || !dec_deg → P3_WCS_PARAM        # :95
中间坐标 (deg): dx=(x+1)−CRPIX_x, dy=(y+1)−CRPIX_y          # :97-98
              ξ = (CD[0][0]·dx + CD[0][1]·dy)·kRad          # :99
              η = (CD[1][0]·dx + CD[1][1]·dy)·kRad          # :100
域守卫（**数值域上界，不是半球界**）:
              r=√(ξ²+η²) ≥ π/2 → P3_WCS_HEMISPHERE           # :524-526
              # r = tan(ρ) = cot θ_native（ρ = 距 CRVAL 的角距，Paper II Table 1
              #   x=R·sinφ, y=−R·cosφ, R=(180/π)·cotθ ⇒ |(x,y)|_rad = cotθ = tanρ）
gnomonic 反投影: θ=atan2(1, r)（=atan(1/r)）                 # :527
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

### 7.3 往返容差（冻结 + 尺度感知分层）

`pixel→world→pixel` 误差 **<1e-8 px**（FP64，**紧门**）——SCI-P3-001 §7
独立不变量 + §9a-12 冻结。**适用域**：`scale ≥ min_scale_arcsec = 0.9″/px`
（覆盖仓内最小真实尺度 0.9586″/px）；低于该尺度时紧门**不适用**（报「超出适用域」，
**不判红**——判红会误拒），退回**全域保守门 1e-6 px**（SCI-WCS-001 §11 STD-F1）。
门值/适用域/证据 = 门表 `docs/science/algorithms/GATES_AND_TOLERANCES.md` §3 的
G-P1-WCS-BRIDGE / G-P1-WCS-BRIDGE-GLOBAL；推导依据
实验/engineering-evidence/（TAN 闭式截断项恒等于 0 ⇒ 误差 100% 来自 FP64 舍入）。
解析oracle回归现状由
独立解析解 oracle 承载（独立解析解，非生产代码
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
| P3_WCS_UNSUPPORTED | 2 | **活跃 fail-closed 出口**（§6.5 表）：frame≠icrs :74 / 未注册码 :99 / 声明放行但未实现 :100 / 无适用域 :438 | ACS_ERR_UNSUPPORTED |
| P3_WCS_HEMISPHERE | 3 | pix2world :524-526 r≥π/2（域上界 ρ*=57.5183634°，非半球界）；world2pix :549-552 denom≤0（真前半球 ρ≥90°）；make 四角守卫透传（:146-155） | ACS_ERR_PARAM |

- 极点守卫单一条件冻结: |dec|≤85°（kMaxAbsDec :15，SCI/API/session
  同一常数）。
- **域守卫与半球界的正确关系（正向约束：两个判据不等价）**：
  - `pix2world` 侧判据是 `r = tan(ρ) < π/2`，即 **ρ < ρ* = atan(π/2) = 57.5183634°**
    （等价于 `θ_native > atan(2/π)`）；它是**数值域上界**（gnomonic 反投影
    `θ=atan2(1,r)` 在 ρ→90°（r→0）附近的精度退化区之前设卡），**不是**半球边界。
  - `world2pix` 侧判据是 `denom = cos(ρ) > 0`，即 **ρ < 90°**（真前半球）。
  - ⇒ `{r<π/2}` ⊊ `{denom>0}`：环带 **57.5183634° ≤ ρ < 90°** 是前半球合法天区，
    但 `pix2world` 一律返回 P3_WCS_HEMISPHERE ⇒ 该环带内正反映射不互逆。
    实测锚（生产码探针；证据正本 = 实验/engineering-evidence/）：
    ρ=57.0°→OK、ρ=57.5184°→HEMISPHERE（阈值 = atan(π/2)）；同一 descriptor 的
    401×401 网格（160801 点）中被 `r≥π/2` 拒绝的 8124 点**全部**满足 `denom>0`。
  - **适用域**：本域合同域 FOV ≤ 20°（门值见本节 max_fov_deg；**§6.1 校验序不含 FOV 项**）的四角 ρ_max ≈ 14.2° < ρ*，
    故该守卫在 alpha 合同域内**永不触发**；它只在视场半对角 > 57.52°（FOV ≳ 115°）时生效。
    该判据的表述口径 = 「视场半对角越界」；用"半球"字样解释它会把 57.52°–90° 的合法天区误判为不可达。
- make 四角守卫透传的返回码可能为 P3_WCS_HEMISPHERE（视场超半球）
  ——即"参数合法但视场越界"仍属失败，不产出 descriptor。

## 10 并发/确定性/资源合同

- 内核纯函数: p3_wcs.cpp grep 实测 0 处 thread/mutex/atomic/omp、
  0 处全局可变状态（匿名命名空间常量+纯函数 :12-28）——const-only
  入口（descriptor 只读）多线程并发安全，descriptor parallel_ok=
  true（module_adapters.cpp:411-427 占位）与此结构性一致。
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
- **kMaxSide 编译期可覆盖**: ACSD_P3_MAX_SIDE（:18-22）——合同
  上限 20000 为默认值语义（PHASE3_API_V1 §2 资源/配置合同，
  :16-17 注释锚），覆盖属构建期显式行为，非静默偏差；如实登记。
- **projection 硬编码**: §6.5——UNSUPPORTED 枚举备而不用；非 TAN
  扩展按 SCI §9a-3 须独立测试并走变更流程。
- **acsd_p3_projection.dll 未建**: entrypoint=MISSING；探针/
  回归现状以独立解析解对拍替代，非 DLL 挂载；由 P3-PROJ-IMPL 建立。
- descriptor 占位词汇（module_id=acsd.phase3.wcs、sci_id=
  SCI-P3-WCS-001、alg_id=ALG-P3-002、test_id=TEST-P3-WCS-001、
  端口 props(DATA-P3-PROPS 必)+wcs_plan(DATA-P3-WCS 可)）为编排层
  词汇（module_adapters.cpp:411-427），由 P3-PROJ-INT 对齐
  acsd.p3.projection；冻结依据唯一 = 本合同。

## 12 TEST-P3-WCS-DESIGN-001 设计冻结（登记面 VERIFIED）

> 三组判据的载体文件**均已不在本仓**（WCS 完整性/尺寸溢出检查、
> 独立解析解回归、四模式探针）；判据本体不变，验收级升级归
> P3-PROJ-TEST。

- **T1 正向解析解**: 已知天球点（含 RA wrap 跨 0/360、|dec|=60°/85°
  边内、四象限 PA∈{0°,90°,−90°,30°}）→ p3_wcs_world2pix → 与
  独立解析解（Calabretta & Greisen 论文 II 形式直接计算）比对，
  容差 <1e-9 px（FP64 机器精度量级）。
- **T2 往返不变量（冻结容差）**: 像素网格全扫描
  p3_wcs_pix2world∘p3_wcs_world2pix 误差 <1e-8 px（SCI §7 冻结值，
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
  P3-PROJ-TEST 建立，与生产实现相互独立）；双平台数值合同按
  backend 数值测试族先例。
- **T7 不变量/回归**: WCS 完整性面与溢出检查保持通过；RAFT: 同入参 1/N worker 结果 bitwise 一致
  （结构性满足，纯函数）。

## 13 合同边界与 DISP 登记

- 本域无 DISP 缺陷登记（现状无已知缺陷；P0 修复已合并，§6.3 如实
  陈述不再登记）。
- 整改项（非缺陷，§11）: PA 接线、dll/入口建立、WCSLIB oracle
  建库——均归 P3-PROJ-IMPL/TEST/INT，本合同层不修码。
- 边界: 不改 vendored 第三方；不改 SCI 公式（§14）；跨域消费
  （p3_session/p3_output）只登记不修；模块页（registry 手写页）=
  docs/detail/registry/acsd.phase3.wcs.md。

## 14 SCI 侧一致性：以独立证据判定，不预设谁为准

- docs/science/PHASE3_HIPS_TO_FITS.md（SCI-P3-001）的 TAN 面（§5 G1/G2、
  §9a-3 TAN-only、§7 容差）仍为 alpha 会话冻结口径，本文件与其一致。
- **订正原则（`docs/ACSD_DESIGN.md` §0（文档权威与索引））**：`docs/science/**` 与
  `docs/science/algorithms/**` 必须科学正确。当独立证据（外部标准/文献/可复跑实验）
  证明文档与标准或事实不符时，**订正文档是义务**；反之文档已被证明正确而
  实现不符时，改实现。**权威方向 = 从 SCI 到实现**：不接受「以代码为准」式权威
  倒置表述；「文档已冻结」只表取值的当前状态，已知错误照改。
- 现行口径：SCI-P3 的 CAR/AIT CRVAL2 语义、AIT 域界、CAR 极行、order/leaf
  公式、coverage/权重容差表述与 `docs/science/unified/DATA_SEMANTICS.md`「方差与逆方差的三态编码」一节的传播式，均以外部标准
  （Paper I/II、astropy/WCSLIB）与可复跑实验为准；正本见
  `docs/science/PHASE3_HIPS_TO_FITS.md` §14/§15。

## 15 projection registry 与 DESIGN §6.3 八投影冻结集合

> **权威依据**：`ACSD_DESIGN.md §6.3`（原文：内置多种投影算法，
> 首批冻结 **TAN / SIN / CAR / AIT / STG / MOL / CEA / ZEA**，每种声明适用域、
> 奇点、经度 wrap、轴手性、CRPIX/CRVAL/CD/PC/CDELT/CTYPE；新增投影经
> projection registry 注册并附独立往返 Oracle）+
> `docs/detail/registry/acsd.phase3.wcs.md`（⑥ 级）+ Calabretta & Greisen (2002) FITS WCS Paper II。
> 投影集合按 DESIGN §6.3 的**八投影**为准。
> **可执行标准**：astropy 7.0.1（WCSLIB）逐点对拍，22 组配置最大球面偏差
> 6.854e-13°；证据与逐项判据见 `docs/science/PHASE3_HIPS_TO_FITS.md` §14/§15。
> **三项科学口径**：
> ① CAR/AIT **把 CRVAL2（含 LONPOLE 标准默认 0/180）纳入映射**——三 Euler 角
> 旋转核（「CRVAL2 仅记录于 header 不进入映射」与 SCI-P3 `CRVAL=center`
> 直接互斥，且使 CRPIX 处 world ≠ CRVAL，违反 Paper I §2.1.1 定义性不变量）；
> ② AIT 域界取 **`A≤1`**（A = xp²/4 + yp²；A=1 即 φ=±180° 边界合法）；
> ③ CAR **native 极行 θ=±90° fail-closed**（整行塌缩：Ω=0、RA 无定义；
> `|θ|>90` 会放行 `=90`，故判据取 `|θ| ≥ 90°`）。
> SCI 侧口径见 §14；本层与 SCI 冲突时按 `docs/ACSD_DESIGN.md` §0（文档权威与索引） 以独立
> 证据判定谁错、改错的一边。

### 15.1 registry 冻结集合、实现状态与版本

- **权威冻结集合 = DESIGN §6.3 八投影，顺序冻结**：
  `TAN / SIN / CAR / AIT / STG / MOL / CEA / ZEA`。registry 导出该集合
  （`registry_frozen_set()` / `registry_is_frozen_code()`），表内 code 必须属于
  该集合（`registry_selfcheck()` 强制；不属于 → 自检失败）。
- **在役 registry = 现行实现** `lib/algorithms/projection/p3_proj.h/.cpp`，
  `kProjectionRegistryVersion = 3`。
- **对照口径实现（离线对照用）**（`lib/algorithms/projection/p3_projection.h/.cpp`，
  `kP3ProjectionRegistryVersion = 1`，`kP3ProjectionRegistryRetired = true`）
  **RETIRED**：仅保留作偏差对照基准（其行为冻结、取值固定，
  偏差集合只减不增，任何变化须复核并更新 §15.9）；
- **实现状态**：已实现 4/8（TAN/SIN/CAR/AIT）；`STG/MOL/CEA/ZEA`
  未实现——`registry_find()` 返回 nullptr（fail-closed，无 fallback），
  实施归 P3-001（GAP-011），逐式公式与独立 Oracle 随后续变更引入
  （本文不臆造未验证公式，只冻结集合成员/适用域声明面）。
- **会话面收窄不变**：alpha 会话仅接受 TAN（SCI-P3 §9a-3 + `p3_wcs_validate_request`）；
  该收窄是**会话层**行为，与 registry 八投影注册面不冲突——registry 面已登记
  冻结集合，会话未接线（见 §15.5 与 SCI §9a-3 注记）。

| 行 | id | code | CTYPE1/CTYPE2 | 中心守卫 | 合法 FOV 声明 | 域 | 状态 |
|---|---|---|---|---|---|---|---|
| 0 | TAN | "TAN" | RA---TAN / DEC--TAN | \|CRVAL2\|≤85° | 20°（SCI §9a-12 冻结） | zenithal, (φ0,θ0)=(0,90°)，LC=180° | 已实现 |
| 1 | SIN | "SIN" | RA---SIN / DEC--SIN | \|CRVAL2\|≤85° | 60°（声明值） | zenithal, (φ0,θ0)=(0,90°)，LC=180° | 已实现 |
| 2 | CAR | "CAR" | RA---CAR / DEC--CAR | \|CRVAL2\|≤85° | <180°（声明值） | cylindrical, (φ0,θ0)=(0,0)，LC 默认 | 已实现 |
| 3 | AIT | "AIT" | RA---AIT / DEC--AIT | \|CRVAL2\|≤85° | 180°（声明值；= 参考点最大球面角距 90° 的**直径**，同表 SIN/CAR 惯例） | pseudo-cylindrical, (φ0,θ0)=(0,0)，LC 默认 | 已实现 |
| 4 | STG | "STG" | RA---STG / DEC--STG | \|CRVAL2\|≤85° | 待 P3-001 声明 | zenithal, (φ0,θ0)=(0,90°) | **未实现** |
| 5 | MOL | "MOL" | RA---MOL / DEC--MOL | \|CRVAL2\|≤85° | 待 P3-001 声明 | pseudo-cylindrical, (φ0,θ0)=(0,0) | **未实现** |
| 6 | CEA | "CEA" | RA---CEA / DEC--CEA | \|CRVAL2\|≤85° | 待 P3-001 声明 | cylindrical（标准纬线 0）, (φ0,θ0)=(0,0) | **未实现** |
| 7 | ZEA | "ZEA" | RA---ZEA / DEC--ZEA | \|CRVAL2\|≤85° | 待 P3-001 声明 | zenithal（等积）, (φ0,θ0)=(0,90°) | **未实现** |

> **「状态」列的层归属（防误读，DISP-P3PROJ-003）**：本表「已实现/未实现」
> 指 **v6 内核**实现面（`p3_proj.h/.cpp`，`kProjectionRegistryVersion=3`；
> 内核 4/8 = TAN/SIN/CAR/AIT），**不是产品声明面**。产品面**唯一权威** =
> `lib/algorithms/projection/p3_projection_registry.h` 的三层模型，其
> **产品受理集 D = I = {TAN}**；SIN/CAR/AIT 在该表状态为 `kKernelOnly`
> （有内核、未接入生产路径）⇒ **不是产品声明，不得表述为产品能力**。
> 与本节 :485-487「会话面收窄不变（仅接受 TAN）」一致。

- 「LC」= LONPOLE 取 Paper II 标准默认：`δ0 ≥ θ0 ⇒ 0°，否则 180°`；实现等价
  形式见 §15.2。八投影全部落在 Paper II 标准集合内（astropy/WCSLIB 8/8 可构造，
  R-1 §2.4）。
- max_fov_deg 为 registry **声明字段**（DESIGN §6.3「每种声明适用域」），**且是 make 期硬门**：
  `lib/algorithms/projection/p3_wcs.cpp` 的适用性检查内有
  `if (!(fov <= ap->max_fov_deg)) { … return P3_WCS_PARAM; }`（TAN 侧 `max_fov_deg = 20.0`，
  同文件标注「SCI §9a-12 alpha 冻结, 禁放宽」）；超限即**拒 make**，不是会话层才判。
  会话层（`lib/phase3_session/p3_export.*`）只是该 make 的调用方，不是 FOV 判定的所在层。
  投影域本身另由四角守卫 + 投影域界（§15.4）承载（与 FOV 门是两道独立判据）。
  口径 = **直径**（球面上离参考点的
  最大合法角距 ×2）：TAN 20°/SIN 60°/CAR 180°/AIT 180°；域界为整球者上限即
  `2×90° = 180°`（改前 AIT=360° 把整球角周长当成了视场直径；R-40/P-081，判据
  `p3_projection_registry_test.cpp` C5b 已锁值与惯例，负例内建）。

### 15.2 统一管线与旋转核（rad 内部计算）

- 正向（world→pixel）: (α,δ) → 旋转核逆 → native (φ,θ) → 投影层 → 中间坐标
  (X,Y) deg → CD⁻¹（解析 2×2，|det|<1e-300 → PARAM）→ 0-based 像素。
  逆向（pixel→world）: 像素 → CD·δp → (X,Y) → 投影层逆 → (φ,θ) → 旋转核 →
  (α,δ)，RA 经 fmod 归一 [0,360)。
- **zenithal 旋转核（TAN/SIN，θ0=90°，LONPOLE=180° 等价式，冻结不变）**：
  `sinθ = sinδ sinδ₀ + cosδ cosδ₀ cosΔα`（=denom，≤0 → HEMISPHERE）
  `φ = atan2(−cosδ sinΔα, sinδ cosδ₀ − cosδ sinδ₀ cosΔα)`
  `δ = asin(sinθ sinδ₀ + cosθ cosδ₀ cosφ)`
  `Δα = atan2(−cosθ sinφ, cosδ₀ sinθ − sinδ₀ cosθ cosφ)`
  （等价于 Paper II §2.2 三 Euler 角取 native 极 = CRVAL 点：δ_p=δ₀、φ_p=180°。）
- **CAR/AIT 旋转核（CRVAL2 进映射）**：投影参考点 (φ0,θ0)=(0,0)，
  LONPOLE 标准默认。由 Paper II §2.2 球面三角形余弦定理解三 Euler 角：
  `sinδ₀ = sinθ₀ sinδ_p + cosθ₀ cosδ_p cos(φ0 − φ_p)`　（θ0=φ0=0 ⇒ cosδ_p = sinδ₀ / cos φ_p）
  `α_p = α₀ − atan2( −cosθ₀ sin(φ0−φ_p), sinθ₀ cosδ_p − cosθ₀ sinδ_p cos(φ0−φ_p) )`
  正映射：`δ = asin(sinθ sinδ_p + cosθ cosδ_p cos(φ−φ_p))`、
  `Δα = atan2(−cosθ sin(φ−φ_p), sinθ cosδ_p − cosθ sinδ_p cos(φ−φ_p))`、
  `α = α_p + Δα`；逆映射（world→native）：`sinθ = sinδ sinδ_p + cosδ cosδ_p cos(α−α_p)`、
  `φ = φ_p + atan2(−cosδ sin(α−α_p), sinδ cosδ_p − cosδ sinδ_p cos(α−α_p))`
  （φ 归一 (−180°,180°]）。
  δ₀=0 ⇒ (α_p, δ_p, φ_p) 退化为恒等旋转（该特例不是一般式）。
- RA 最短角差仅在 zenithal 逆核与 RA 归一中使用；CAR/AIT 由旋转核的
  `cos/sin(α−α_p)` 自然承担 wrap，无独立最短角差分支。

### 15.3 投影层（已实现四投影逐式冻结；未实现四投影见 §15.1）

- **TAN**（§6/§7 冻结零改动，冻结逐式路径）: R=cotθ；
  X=−R sinφ, Y=R cosφ。逆: r=√(X²+Y²) (rad)，r≥π/2 → HEMISPHERE；
  θ=atan2(1,r)，φ=atan2(−X,Y)。与 lib/algorithms/projection/p3_wcs.cpp 生产实现
  bitwise 一致（§15.6 T3 对拍承载）。
- **SIN**（orthographic）: R=cosθ；X=−R sinφ, Y=R cosφ。逆:
  ρ=√(X²+Y²) (rad)，ρ>1 → HEMISPHERE（ρ=1 边界合法，θ=0）；
  θ=acos(ρ)，φ=atan2(−X,Y)。奇点声明：半球边界圆 ρ=1；中心守卫
  |CRVAL dec|≤85° 为保守收窄（SIN 极视场数学可行，冻结域不含）。
- **CAR**（plate carrée）: X=φ, Y=θ（Paper II Table 1，y=+θ：declination 随 y
  增加）。逆: φ=X·kRad, θ=Y·kRad；
  **`|θ| ≥ 90° → PARAM`（native 极行 fail-closed：θ=±90° 是整行塌缩——
  Ω=0、RA 无定义，`|θ|>90` 会放行 `=90`，故判据取 `|θ| ≥ 90°`）**；world→native 端同一条件
  （native 极点上 φ 不唯一 ⇒ PARAM）。高纬面积畸变由 FOV 声明承载。
- **AIT**（Aitoff）: 正向 D=√(1+cosθ·cos(φ/2))；γ=√2/D；
  X=2γ·cosθ·sin(φ/2), Y=γ·sinθ（Paper II 标准，含 √2）。逆: 令 xp=X/√2, yp=Y/√2，
  **A=xp²/4+yp²，`A>1 → HEMISPHERE`（A≤1 为 Paper II 椭圆域，半轴
  X=2√2 rad=162.0569°、Y=√2 rad=81.0285°；判据 A<2 会多接受
  |X|≤229.125° 的折叠环带）**；sinθ=yp·√(2−A)，|sinθ|>1 → HEMISPHERE；
  θ=asin(sinθ)，φ=2·atan2(xp·√(2−A)/2, (2−A)−1)（A=1 即 φ=±180° 边界合法，
  atan2 唯一）。**可构造域**：
  AIT 的可构造矩形帧族 = **椭圆内接矩形族**，上限 **229.24°×114.56°**（面积 8.000 sr = 4π 的 **63.7%**，
  与解析最优 2ab = 8 rad² 吻合）；**全天空（360°×180°）不可构造**——四角守卫下 360×180 帧四角
  A = xp²/4 + yp² = **1.994 > 1** ⇒ 必判 HEMISPHERE（720×360 → 1.997）。性质：椭圆内接矩形四角恒在椭圆上
  （A=1），而覆盖整个椭圆的矩形四角恒在椭圆外（A=2>1）⇒ 该限制**数学上不可达**，不是实现缺陷。
  「全天空」只作展示语义（投影自身定义域），**可构造 FOV 声明面不含该表述**。
- **STG/MOL/CEA/ZEA**：只登记集合成员与适用域（§15.1 表）；逐式公式 +
  域界 + 独立往返/绝对对拍 Oracle 随实现引入并在本节追加。
  四者均属 Paper II 标准集合（astropy/WCSLIB 可构造，R-1 §2.4）。

### 15.4 descriptor/make 守卫与域界语义

- make 参数校验序与 TAN 冻结序一致（§6.1）: out 非空 → id 注册校验
  （越界/未实现 → UNSUPPORTED）→ parity∈{east_left,east_right}（nullptr 归一
  east_left）→ |centre_dec_deg|≤85°（已实现四投影统一保守冻结，TAN 侧=SCI
  单一条件）→ scale>0 → W,H∈[1,20000]（kMaxSide 默认，可
  ACSD_P3_MAX_SIDE 编译期覆盖）→ G1 CD 构造（§6.2 逐式，四投影同构）→
  四角投影域守卫（0-based (0,0)/(W−1,0)/(0,H−1)/(W−1,H−1) 逐一调投影域检查，
  任一失败 → 首败码透传，不产半成品 descriptor）。
  四角守卫对 CAR 即「禁触碰 native 极行 |θ|≥90°」；对 AIT 即「四角 A≤1」。
- 状态码（P3ProjectionStatus，值域与 P3WcsStatus 冻结对齐）: OK=0 /
  PARAM=1 / UNSUPPORTED=2（未知码/越界 id/冻结集内未实现） / HEMISPHERE=3。
- 域界语义: TAN r≥π/2 / SIN ρ>1 / AIT A>1 → HEMISPHERE；
  CAR native 极行 |θ|≥90° → PARAM；TAN/SIN world2pix 端 |dec|>85° → PARAM
  （§9 冻结语义沿用）。
- fits_keywords: CTYPE1/2 经 registry spec 解析（禁硬编码 CTYPE 于
  调用方），其余行与 §8 同族（CRPIX/CRVAL %.10f、CD %.12e、每行
  ≤80 字节、12 行）；descriptor 空指针/未注册 id → 空串（fail-closed
  不产 CTYPE 面）。CRVAL2 为**映射量**，参与投影映射；LONPOLE 采用
  Paper II 标准默认语义，读方无需额外关键字即可复现映射。

### 15.5 六要素声明（DESIGN §6.3 逐投影）

| 投影 | 适用天区 | 奇点 | 经纬方向 | CRPIX/CRVAL/CD/CTYPE 规则 | 合法 FOV | 独立往返 Oracle |
|---|---|---|---|---|---|---|
| TAN | \|CRVAL dec\|≤85°, 视场同半球 | 天顶反面 r≥π/2 | parity 显式（§9a-4） | §6/§7 冻结；CRVAL=切点（含 CRVAL2） | ≤20°（SCI 冻结） | 3D 向量 gnomonic 透视重建（§15.6 T2/T4） |
| SIN | \|CRVAL dec\|≤85°, 半球内 | 半球边界 ρ=1 | parity 显式 | 同 G1；CRVAL=投影点（含 CRVAL2） | ≤60°（声明值） | 3D 向量 orthographic 重建 |
| CAR | \|CRVAL dec\|≤85°, native \|θ\|<90° | **native 极行 θ=±90°（整行塌缩：Ω=0、RA 无定义；fail-closed）** | parity 显式 | 同 G1；**CRVAL1/CRVAL2 均进映射**（§15.2） | <180°（声明值，球面行跨度） | 三 Euler 角独立式 + astropy 绝对对拍 |
| AIT | \|CRVAL dec\|≤85°, 椭圆域 A≤1 | 椭圆域边界 A=1（φ=±180°）+ native 极 θ=±90°（非塌缩：Ω>0） | parity 显式 | 同 G1；**CRVAL1/CRVAL2 均进映射** | ≤180°（声明值，**直径惯例** = 参考点最大球面角距 90°×2；椭圆域半轴 162.0569°×81.0285° 是**平面**像量，不参与本字段）；**可构造矩形帧上限 229.24°×114.56°（63.7% 天空）** 同属平面量，全天空帧在四角守卫下必拒（§15.3） | Paper II 反演独立式 + astropy 绝对对拍 |
| STG/MOL/CEA/ZEA | 待 P3-001 声明（zenithal/pseudo-cylindrical/cylindrical/zenithal 等积） | 待声明 | 待声明 | 同 G1（占位） | 待声明 | 待建立（每投影独立 Oracle，DESIGN §6.3 硬要求） |

- 「保守收窄」：已实现四投影中心守卫统一沿用 85° 单一条件（与 TAN 同值），
  属冻结域收窄（alpha 原则），极点中心视场排除；放宽须经变更流程。
- parity/PA 语义四投影统一（§6.2 冻结式原样）：CD 构造与投影无关
  （G1 对角 + PA 推广 + P0 修复内含）。

### 15.6 TEST-P3-PROJ-REG-001（判据清单冻结）

- **T1/T2 名与判据分工**：往返自洽 ≠ 标准正确。现状实现在 30° 错
  映射下往返误差仍 4.5e-15 px ⇒ 只保留「往返」会恒绿。故拆两条：
  - **(i) 往返不变量**（保留）：每投影 pixel→world→pixel < 1e-8 px；
  - **(ii) 绝对标准对拍**（新，验收口径）：astropy/WCSLIB 逐像素对拍，
    容差 ≤1e-8 deg（实测 ≤6.854e-13°），**必须含 dec0≠0 用例**（CRVAL2 只在
    δ0≠0 进入映射；δ0≡0 对 CRVAL2 缺陷零区分力）与 **CRPIX↔CRVAL 定义性
    不变量**（pix2world(CRPIX)==CRVAL，Paper I §2.1.1）。
- 可执行面（**已撤**：下列判据由实验复算与人读对抗审核执行，判据本体不变）：
  - `p3_proj_test.cpp`: T1 registry 完整性（版本=3/已实现 4 行/冻结集合 8 行
    且 code 顺序/DESIGN 集合成员判定/函数指针/selfcheck=0/未知码 nullptr 无
    fallback）；T2 往返；T4 3D 向量独立解析解（TAN/SIN）+ CAR dec=+Y/AIT γ=√2
    独立式；T5 G1 CD 精确断言；T6 负面清单（未知码/parity 非法/|dec|=85.1/
    scale≤0/W=0/H=20001/空指针全族/SIN 视场超半球/**AIT A>1（含折叠环带
    |X|=229.125°）**/**CAR native 极行 |θ|=90 与 make 触极行**/SIN 背面点）；
    T7 CTYPE 关键词面；T8 确定性；T9 1/N worker；
    **T-H CRVAL2 进映射**：CAR/AIT dec0∈{±30,+60,−45} 的
    `pix2world(CRPIX)==CRVAL` 与「dec ≠ 平面 Y」区分性断言。
  - `p3_proj_wcs_oracle.py`（astropy 独立实现逐点绝对对拍，正例臂）: astropy 独立实现逐点绝对对拍（14 条用例，含 6 条
    dec0≠0）+ 逐像素 Ω 盈余 + CRPIX↔CRVAL 不变量 + **AIT 椭圆域 A≤1 判据**；
    CAR 解析纬度带判据限定 `|CRVAL2|≤1e-9`（倾斜 CAR 的行不是天球纬度带，
    否则对任何正确实现都误判，R-1 §4-B）。
  - **对照口径偏差表判据**（§15.9 四项，对照口径实现（RETIRED））⇒ 表内偏差必须复现（缺失即红，须复核），表外新偏差亦红。
- 故障注入（必败面，测试级注入、生产源零 getenv）:
  `ACSD_P3PROJ_FAULT=const_omega|legacy_car|legacy_ait|swap_norm|naive_wrap`
  注入等价缺陷，注入模式断言必败并报告捕获（FAULT-EFFECT-CONFIRMED）。
- 验收级 oracle 升级（WCSLIB 独立实现，§12 T6）仍归 P3-PROJ-TEST，本层不冒认；
  现行注册表已把「绝对对拍 + dec0≠0 + CRPIX 不变量」落到 Oracle 可执行面。

### 15.7 构建挂载与越界登记

- 生产构建挂载（acsd_p3_projection.dll target/adapter 接线/会话消费）归
  P3-PROJ-IMPL/P3-002；根 CMakeLists.txt / lib/phase3_session 零改动——本层不碰生产构建面。
- registry 现状**非生产构建成员、非 DLL 入口**；
  module.yaml 维持 CONTRACT_READY/entrypoint=MISSING 不冒认
  IMPLEMENTED（升级归挂载任务）。

### 15.8 文献锚

- Calabretta & Greisen 2002, A&A 395, 1077（Paper II）§2.1（投影层
  x,y 与 native 坐标）、§2.2（celestial↔native 三 Euler 角旋转与 LONPOLE
  默认规则）、Table 1（各投影 native 层）；CAR/AIT 的 CRVAL2 进映射与 AIT
  椭圆域 A≤1 即出此。
- Greisen & Calabretta 2002, A&A 395, 1061（Paper I）§2.1.1：CRPIX↔CRVAL
  定义性不变量（中间坐标为 0 的像素其天球坐标 = CRVAL）；CD-only/CTYPE/
  CRPIX 1-based 约定（既有 §5 锚不变）。
- **可执行标准（全文获取受阻时的替代口径，R-1 §2.0）**：astropy 7.0.1
  （WCSLIB）逐点对拍——22 组 (proj, α0, δ0∈{0,±10,±30,±60,±85}, parity, PA)
  最大球面偏差 6.854e-13°；AIT 边界实测 X=162.0560°（标准 162.0569°）、
  Y=81.0280°（标准 81.0285°）；CAR 极点行 astropy 与现行实现一致（fail-closed
  是项目产品语义选择：该行 Ω=0、RA 无定义）。
- 3D 向量 oracle 第一性原理（切基正交投影/透视除法）为 Project-defined
  独立推导路径，与生产球面三角公式互为独立验证。

### 15.9 对照口径偏差表（离线对照用，RETIRED 冻结对照）

> 对照口径 = `lib/algorithms/projection/p3_projection.h/.cpp`（RETIRED）。四项偏差
> 断言「仍然复现」；修好 v1 或偏差消失 ⇒ 该判据转红（须复核并更新本表）。
> 实测读数（rc=0）:
> D1 = 60°（=|CRVAL2|）; D2 = 345600″; D3 = 94885″; D4 = 63.3°。

| # | 偏差 | 判据 | 状态 |
|---|---|---|---|
| D1 | CAR/AIT 的 CRVAL2 不进映射（world(CRPIX) ≠ CRVAL，偏差 ≈ \|CRVAL2\|） | CRPIX 行 world 与 CRVAL 之差 ≥25° | 冻结对照 |
| D2 | CAR 用 Y=−θ（declination 反号：dec_v1 = −dec_标准） | 反号残差 <1e-6″ 且标准角距 ≥3600″ | 冻结对照 |
| D3 | AIT 缺 Paper II γ 的 √2 因子（平面尺度差 √2） | 同像素天球位置偏移 ≥3600″ | 冻结对照 |
| D4 | AIT 域判据 A<2（正确 A≤1；接受 \|X_v1\|>2 rad 的折叠环带，\|ΔRA\| 折返） | 折返幅度 ≥5° | 冻结对照 |

## 16 登记面：C1–C9 的实现侧缺口（**只登记，不改码**）

> 依据：`ACSD_DESIGN.md` §6.3（导出只接受面亮度语义输入）+ §11.1.1 第 8 条（未满足 ⇒ 如实登记为未验证）；
> 证据：Phase2 信号量纲判据正本 = `docs/science/PHASE3_HIPS_TO_FITS.md` §16（C1–C9）；
> 本节**只登记**实现侧缺口与归属，**不改动**任何公式、阈值、容差、锚点与冻结集合；科学侧口径见 `docs/science/PHASE3_HIPS_TO_FITS.md` §16（同一实验单元，文字只有这一份）。

| # | 位置 | 现状（实测） | 归属 |
|---|---|---|---|
| C1 | `docs/detail/registry/acsd.phase2.write.md:51` | `UnitId::ADU（signal surface brightness）` | ✅ 现行 `UnitId::SURFACE_BRIGHTNESS` |
| C1b | 同页 `:63-64` / `lib/algorithms/coverage/hips_p2/README.md:100` / `lib/algorithms/coverage/hips_p2/module.yaml:33` | 行锚 `module_adapters.cpp:1166-1188` / `:677-694` 已漂移 | ✅ 现址 `:1040-1057`（`grep -n p2_write_descriptor → :1040`） |
| **C1a** | `lib/infrastructure/scheduler/src/module_adapters.cpp:1040-1057`（`p2_write_descriptor`） | `mosaic` 端口仍 `UnitId::ADU`（:1049），`integrated` 亦为 `UnitId::ADU`（:1048）；`UnitId::SURFACE_BRIGHTNESS` 枚举已存在但 phase2 未用 | **lib/** ⇒ FIX / P2-XX-INT（本包只登记） |
| C2 | `docs/detail/registry/acsd.phase2.write.md` 的写出链「逆归一」小节（`acsd.phase2.write.md:41`） | writer 视图中间量 `flux` 与产品语义混淆 | ✅ 已补「该 `flux` 是 writer 视图中间量、落盘值 = `flux_sum/covered_area`」 |
| C3 | `docs/science/unified/DATA_SEMANTICS.md` §3.4 | `ADU surface brightness` 措辞歧义 | ✅ 已明确为 `ADU/sr` 并登记「产品 tile 无 `BUNIT`、properties 无像素语义 provenance」 |
| **C4** | `lib/phase3_session/p3_session.cpp:166-172,396` + `CMakeLists.txt:759-760` | export **无**输入语义守卫：只透传 BUNIT（缺省 "ADU"）；守卫内核 `p3_rsmp_units.cpp:137-171` 与会话接线层 `p3_export.cpp` **未进构建**（`grep -c p3_export CMakeLists.txt` = **0**） | **lib/** ⇒ FIX / Phase3 export 域（本包只登记；§6.3 生效与否以接线实测为准） |
| **C5** | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` finalize | signal 产品不写 `BUNIT="ADU/sr"`，properties 无 `pixel_semantics`/`pixel_area_power` ⇒ 即使接线，当前产品会被自己的守卫 REJECT | **lib/** ⇒ FIX（本包只登记） |
| C6 | 上游 P1 产品 | 真实 Phase1 `signal` 含 `±1e14–1e15` 量级值（低覆盖像素分母退化） | P1 域单独处理（登记） |
| C7 | 实验内部判据（非生产文档） | 预注册把舍入预算 `τ=2e-6` 用于像素化主导的统计量 | 后续实验（登记） |
| C8 | `docs/detail/registry/acsd.phase2.write.md` | 未说明「输入 support 恒为 1 时 `acsd_support_clamped_pixels` 也非零」 | ✅ 已补注（实测常量场 = 262144） |
| **C9** | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:495-499,776-800` | hierarchy 归约在 **f32** 累加器上做：dk=1 逐位精确、dk=9 偏差 **2.5e-3**（合成）/ **3.95e-4**（真实）；`f32_accum_repro.json` 复现发布值到 1.5e-9，float64 理想值差 2.52e-3 | **lib/** ⇒ FIX（本包只登记；修法 = `sumFluxD/sumAreaD` 分支或 Kahan/分块补偿求和） |

- **判据冻结**：`docs/science/PHASE3_HIPS_TO_FITS.md` §16 结论表 + 实验/engineering-evidence/ 的 `results/PREREGISTRATION*`。
- **负例（判据非退化）**：FLUX-IN 支同二进制下 `R_cross = 4.0`、`T ≈ −1`；面积标度错注入 `T = −0.75` ⇒ 判据能红能绿。
- **不在本节范围（已知偏差，现行）**：现行 SIN 内核用 `ctheta = sqrt(1 − stheta²)` 反算，存在**灾难性消去**，往返误差**无上界**（0.5″/px 实测 2.5e-5 px，超 SCI §7 冻结容差 1e-8 px 三个量级以上）。该内核**生产不可达**（生产注册表仅 TAN 可用）；其已知偏差不入生产判据；入库复现判据在 SIN 内核修好（`ctheta` 改用数值稳定形式）时由绿转红，须复核 §15.9 偏差表。

## 参考文献与参考代码库（含许可证）

- 投影 native↔celestial：Calabretta & Greisen 2002, A&A 395, 1077（Paper II）§2.1/§2.3 式(2)–(7)/§5.1.3 式(54)（旋转矩阵形式见附录 A.1）；本文件 §15.8 已给逐条文献锚，本节只补代码库。
- CRPIX/CRVAL 不变量：Greisen & Calabretta 2002, A&A 395, 1061（Paper I）§2.1.1 式(1)（q_i=Σ_j m_ij(p_j−r_j)）与 §2.1.4（1-based）。
- 可执行标准：astropy 7.0.1（BSD-3-Clause）/WCSLIB（LGPL-3.0）逐点对拍（R-1 §2）。
- 各投影原始定义（TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA）见 Paper II Table 12（投影代码表）及其引用（Aitoff 1889；Mollweide 1805；Lambert 1728–1777 等）、各投影公式见 §15.3 投影层（逐式）与 §15.2 统一管线与旋转核；Astro Celestial Sphere Database（ACSD） 逐式以 Paper II 为准。
- 3D 向量 oracle：Project-defined 第一性原理推导（§15.8）。


---

## U 承接：`uncertainty_available=false`（fail-closed 唯一出口）

本层产出/消费不确定度子产品时，`uncertainty_available=false` 的处置**承接 《产物与清单登记面》 的 fail-closed 唯一出口**（DATA-UNC-001 规则行与 DATA-P2-PROV-001 显式登记行，按**内容锚**读、不按行号读）：不写 variance/ivar 子产品，manifest 写 `uncertainty_available=false` 并在 diagnostics 标红计数。该键不是失败态，是 unavailable 的显式登记模式；键名与取值口径以该处为唯一权威，本层不另立第二套。
