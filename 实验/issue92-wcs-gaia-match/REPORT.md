# Issue #92 报告：Gaia 逆投影 0/2309 → `PHOT_FIT_NO_SCALE` 首个错误定位

- Issue: [#92](https://github.com/fujiaze/Astro-CS-Database/issues/92)
- 排期: [#22 comment 6229749421](https://github.com/fujiaze/Astro-CS-Database/issues/22#issuecomment-6229749421) 的 **A1 首批**（定位 + 可复跑负例为界）
- 冻结 main: `58ee3fe03fe650ae4fce6e0aea6c2fd65dd6bcfd`（= `origin/main`，工作树无 tracked 改动）
- 生产二进制: `build/linux-sci-p5/acsd` `sha256=29d991f5aff87277416c8fd4331b8f8906c020a9b9a27f2ed2b353bbb24bc90b`
- 本批**未改任何生产源码**；只新增 `实验/issue92-wcs-gaia-match/**`（实验、探针与证据）

---

## 0. 结论（一句话）

`star_matcher` 的 Gaia 逆投影 `skyToPixel` 对**每一颗**星返回 `NaN`，因为消费者在「显式 AP/BP 多项式逆」分支上，把**未精化**的一次性多项式结果放进了一个 **1e-7 px 绝对容差**的前向残差门：
`solver` 产出的 `AP_0_0=-4.34e-5`（切点处残差即 `4.34e-5 px`）与 `AP/BP` 被截断到 `sip_order=3`（帧角落残差 ~0.7 px）**同时**超过该门，于是 2309/2309 被拒 → `gaia_projected_in_frame=0` → `n_matched=0` → `PHOT_FIT_NO_SCALE`。

**这是消费者侧的 fail-closed 门禁误杀一个可用的逆变换（判定 (c)），不是坐标/单位/像素基准错误，也不是几何投影错误。** 前向映射（含 SIP A/B）由独立 WCSLIB/Astropy oracle 与产品自带 `samples[]` 验证到 `1e-10 arcsec` 量级一致。

**根因位置**：`lib/algorithms/photometry/cpp/src/wcs_transform.cpp:260-271`（`if (m_has_sip) { ... > 1e-7 ... }`，NaN 在 `267-268`），触发输入是该文件 `232-238` 的显式 AP/BP 分支。

---

## 1. 冻结身份与真实输入

见 `evidence/00-identity.json`。

| 项 | 值 |
| --- | --- |
| git HEAD | `58ee3fe03fe650ae4fce6e0aea6c2fd65dd6bcfd` |
| origin/main | 同一 SHA |
| tracked 脏文件 | 无 |
| 生产二进制 | `build/linux-sci-p5/acsd`，`29d991f5…`，10 125 864 B |
| 运行配置 | `/workspace/issue92-scratch/cfg6_issue92.json`（`实验/issue12-simpson-domain/evidence/50-cfg6.json` 的副本，仅改 `output_dir`），`sha256=33b9a8e3…` |
| 真实六帧 + 三张 master | 逐文件 `sha256` + `bytes` 见 `00-identity.json`（共 9 个输入，单帧 33 566 400 B，master 67 133 440 B） |
| Gaia 星表 | `gaia/GaiaDR3SP`，20 个 XPSD，67.8 GB；**全量 sha256 = NOT_RUN**（已记 name+bytes 清单，见 identity） |
| 既有冻结产物 | `50-cfg6.json sha256`、`/workspace/issue12-scratch/norm6_pre.p1-work/…/p1_wcs.json sha256`（均在 identity 内） |

原始 FITS 只读、未修改、未上传；入仓的只有派生 JSON/CSV 与脚本。

---

## 2. 真实 CLI 复现：FAIL（与 #12 证据逐项一致）

命令（真实生产 CLI，`photometry.fit.enabled=true`）：

```bash
build/linux-sci-p5/acsd normalize --json /workspace/issue92-scratch/cfg6_issue92.json -y
```

| 观测 | 本批复现 | #12 证据 |
| --- | --- | --- |
| `exit_code` | **4** | 4 |
| 最终状态 | `phase1_failed / upstream_status=PHOT_FIT_NO_SCALE` | 同 |
| `gaia_projected_in_frame`（6 帧） | `0/2309, 0/2309, 0/2306, 0/2307, 0/2310, 0/2307` | 同 |
| `n_matched`（6 帧） | 全 0 | 全 0 |
| `p1_phot.json.photscal` | `1.0`（占位，非拟合值） | 同 |
| `n_frames_applied / failed` | `0 / 6` | 同 |
| `p1_wcs.json` | `n_pairs=54,53,52,50,57,54`；`rms_arcsec=0.127,0.134,0.116,0.123,0.124,0.107` | 帧1 54 / 0.127 |

证据：`evidence/40-cli-repro.json`、`40-cli-repro.keylines.txt`、`40-p1_phot-issue92.json`、`40-p1_wcs-frame1.json`。

> `p1_wcs.json` 有真实解（`rms≈0.13″`），而投影入帧为 0 —— 说明坏点在「解算产物 → star_matcher 消费」之间，不在解算本身。

---

## 3. 生产调用链与「star_matcher 真正消费的投影对象」

```
p1_op_wcs                 → <work>/<frame>/p1_wcs.json  ("wcs" 对象: crpix/crval/cd/ctype/sip)
module_adapters.cpp:6252-6308
    read p1_wcs.json["wcs"] → wj
    p1_wcs_astrometry_usable(wj)         # 有限 crval + 非退化 CD (不解 SIP)
    p1_parse_sip(wj)                     # order/ap_order/a/b/ap/bp
    freq.crpix1/2, crval1/2, cd11..22, sip_order = sip.order,
    freq.sip_a/b/ap/bp = sip.{a,b,ap,bp}          # 6303-6308
frame_photometry_fit.cpp:266
    pc_calibrate_simple_with_gaia_f64_v2_qf(...)  # 传 crval 作锥心, fov_radius 作半径
pc_api.cpp:823-825
    pc::WcsTransform(crval1,crval2, crpix1,crpix2, cd..,
                     sip_order, sip_a,sip_b,sip_ap,sip_bp)
pc_api.cpp:832-836
    StarMatcher::matchWithKdTree(wcs, gaia_ra,dec,..., 2.0, diag, width, height)
star_matcher.cpp:207-218
    wcs.skyToPixel(gaia_ra[i], gaia_dec[i], px, py) → [0,W)×[0,H) 计数
```

消费对象 = `pc::WcsTransform`（`wcs_transform.h:8-12` 声明：CRPIX 1-based、像素 0-based、天球为度）。
`pc_api` 只把 `sip_order`（= `sip.order`=3）传下去；`sip.ap_order`=5 **不进入消费者**。

真实 CLI stderr 里消费者自己打印了构造入参（`[wcs_transform] 初始化:`，本批共 29 928 行），逐帧与产物对照。

---

## 4. 逐字段对照（生产者 vs 消费者运行期入参）

见 `evidence/30-field-compare.json`、`30-wcs-consumed-frame1.json`。容差 = 日志打印格式的量化步长（消费者日志只印 4–7 位有效数字），非数值不一致。

| 字段 | 生产者 `p1_wcs.json` | 消费者构造日志 | 判定 |
| --- | --- | --- | --- |
| `crpix1/crpix2` | 2048.5 / 2048.5 | 2048.500 / 2048.500 | ✅ |
| `crval1/crval2` | 83.283136062 / -6.375372027 | 83.283136 / -6.375372 | ✅（打印精度内） |
| `cd11..cd22` | `-1.6239e-6 …` | `[-1.624e-06, 2.663e-04; -2.663e-04, -1.617e-06]` | ✅ |
| `det(CD)` | 7.090e-08 | 8e-08（打印）| ✅ 非退化 |
| `ctype1/2` | `RA---TAN-SIP` / `DEC--TAN-SIP` | （不透传给 WcsTransform；SIP 由数组决定）| ✅ 语义一致 |
| `sip_order` | 3 | 3 | ✅ |
| `sip.ap_order` | **5** | **未传** | ⚠️ 丢失（见 §7 次因） |
| 像素基准 | `pixel_origin=0-based`，`fits_pixel_origin=1.0`，`samples[].x/y∈[0,4095]` | `dx = x-(crpix-1)`，输出 0-based | ✅（Astropy `origin=0` 复现 samples 到 1e-10″） |
| 图像尺寸 | 4096×4096 | `freq.width=fim.w()=4096`，入帧判据 `[0,4096)` | ✅ |
| ICRS 角度单位 | 度 (J2000) | 度（`D2R`） | ✅ |
| ICRS/SIP 数值语义 | 产品 `samples[]`（含 A/B 前向） | 同一 A/B 前向 | ✅ 见 §6 |

**没有发现任何字段被错误翻译、换序、差 1 或错单位。**

---

## 5. 真实 Gaia 星的投影探针（post vs pre，同一驱动、只换一个文件）

`evidence/20-wcs-probe-frame1.json`。真实 Gaia 锥形搜索复刻生产口径（锥心 = crval、半径 0.9255°、`mag_min=6`、`mag_max` 自适应 {12..16} 至 `n≥2000`），**得到与生产完全相同的 2309 颗星**（`40-gaia-stars-frame1.csv`）。同一份 `wcs_probe.cpp` 两遍编译，唯一变量 = 被编译的 `wcs_transform.cpp`：

| 构造 | post（当前 main） | pre（`38eaeba8b^`，即 #56/#57/#61 修复前） |
| --- | --- | --- |
| 真实 AP/BP（生产调用面） | **n_nan=2309 / n_in_frame=0** | n_nan=0 / **n_in_frame=1085** |
| 同 AP/BP，传 `nullptr,nullptr`（不动点分支） | n_nan=0 / n_in_frame=1085 | n_nan=0 / n_in_frame=1085 |
| 纯线性（无 SIP） | n_nan=0 / n_in_frame=1100 | 同 |

`pre` 下像素→天球→像素往返：切点误差 `3.7e-5 px`，角落 `(0,0)` 误差 `0.51 px`（AP/BP 一次式本身的精度）；`post` 下该往返**全部 NaN**。

> 只换 `wcs_transform.cpp` 一个文件即把 1085 → 0。`38eaeba8b`（#56/#57/#61 修复）**只改这一个文件**（`git diff --stat` 证实：1 file changed, 38+/25-）。

---

## 6. 独立 WCSLIB/Astropy oracle（不复用生产向量数学）

`evidence/10-astropy-oracle-<frame>.json`（六帧全做），真实 WCS + 产品自带 64 个 `samples[]`：

| 检查 | 结果（六帧） |
| --- | --- |
| Astropy 前向 `all_pix2world(x,y,origin=0)` vs 产品 `samples[].ra/dec` | max **9.2e-11 – 1.2e-10 arcsec** |
| numpy clean-room（独立实现）前向 vs samples | max ~5e-11 arcsec |
| Astropy 逆向 `all_world2pix`（带 AP/BP 初值 + 迭代） | max **2.8e-7 – 3.9e-7 px** |
| Astropy 逆向（去掉 AP/BP 迭代） | 同上 |
| 切点自检 `all_pix2world(crpix-1)` | err 3.2e-12 arcsec |
| **一次性**应用 AP/BP（复刻生产 C++ 分支，AP 按 `sip_order=3`） | 残差 max **0.63–0.71 px**，中位 0.016–0.036 px，**64/64 超 1e-7 门** |
| 同上但 AP 按真实 `ap_order=5` 求值 | 残差 max 1.3e-3 – 1.6e-3 px，**仍 64/64 超 1e-7 门**（切点处即 `|AP_0_0|=2.9e-5–4.3e-5`） |

前向被独立确认正确 ⇒ 不是投影几何、不是 CD/SIP 前向、不是像素基准或角度单位问题。

### 6.1 端到端交叉核对（不依赖 C++ 探针）

`evidence/50-astropy-crosscheck.json`：把**同一批 2309 颗真实 Gaia 星**直接喂给 Astropy：

| 量 | 值 |
| --- | --- |
| Astropy `all_world2pix` 后落在 `[0,4096)²` 的星数 | **1085** |
| C++ 不动点分支（post）入帧数 | **1085**（一致） |
| C++ pre（`38eaeba8b^`）入帧数 | 1085 |
| 生产一次式 AP/BP 分支前向残差 | **2309/2309 > 1e-7 px**（min 1.03e-5，中位 0.269，max 1.84） |

即：**投影本身把 1085 颗星正确放进帧内**（两个完全独立的实现给出同一个数），而门把 2309 颗全部判为无效。

---

## 7. 首个错误位置与根因判定

### 7.1 首个错误位置

`lib/algorithms/photometry/cpp/src/wcs_transform.cpp:216-274`：

```cpp
if (m_has_sip) {
    if (m_sip_ap != nullptr && m_sip_bp != nullptr) {          // 232-238: 一次性 AP/BP, 不精化
        double fp = evalSip(m_sip_ap, dx, dy, m_sip_order);    // 235: 注意用 m_sip_order
        double gp = evalSip(m_sip_bp, dx, dy, m_sip_order);    // 236
        dx += fp; dy += gp;
    } else { /* 64 轮不动点 */ }
}
if (m_has_sip) {                                               // 260-271: 前向残差门
    const double check_x = dx + evalSip(m_sip_a, dx, dy, m_sip_order);
    const double check_y = dy + evalSip(m_sip_b, dx, dy, m_sip_order);
    if (!std::isfinite(check_x) || !std::isfinite(check_y) ||
        std::fabs(check_x - distorted_dx) > 1e-7 ||            // 265 ← 首个错误比较
        std::fabs(check_y - distorted_dy) > 1e-7) {            // 266
        x = std::numeric_limits<double>::quiet_NaN();          // 267
        y = x; return;                                          // 268
    }
}
```

在 `232-238` 的一次性分支上，`dx = u + AP(u,v)`；门要求 `dx + A(dx,dy) == u` 到 `1e-7 px`。
消费者用 `m_sip_order`（= `sip_order`=3）求 AP/BP，而生产者 `ap_order=5`。

**关键：这条错误分支在生产路径上不可绕过。** `P1SipCoeffs::{a,b,ap,bp}` 是固定 `double[36]`（`module_adapters.cpp:4605-4613`），`freq.sip_ap/sip_bp` 因而**恒为非空指针**（只要 `p1_wcs.json` 有 `sip` 对象）；`pc_api.cpp:823-825` 把它们原样交给 `WcsTransform`，于是 `:233` 的 `m_sip_ap != nullptr && m_sip_bp != nullptr` **恒真**，`:244-254` 那段能精确反演（残差 ~1e-13 px）的不动点回退**在本调用面不可达**（只有完全无 SIP 时才走不到 AP 分支）。

**解析触发式（切点处，`u=v=0`，`A(0,0)=B(0,0)=0`）**：

```
residual_center = |dx + A(dx,dy) - u| = |AP_0_0| = 4.3348e-5 px  >  1e-7 px  → NaN
```

即在视场正中心也已经越门；帧角落再由「AP 被截断到 3 阶」放大到 ~0.7 px。

### 7.2 量化：哪个变量是必要且充分的

`evidence/20-project-variants.json`，在**同一批 2309 颗真实 Gaia 星**上复刻同一数值、逐个切换变量：

| 变体 | gate 通过 | 入帧 | 残差 max | 残差中位 |
| --- | --- | --- | --- | --- |
| 生产原样（AP@order3，不迭代） | 0 | **0** | 1.84 px | 0.269 px |
| 只把 `AP_0_0=BP_0_0=0`（不迭代） | 0 | 0 | 1.84 px | 0.269 px |
| AP/BP 全零（不迭代，等价「解析到 sip 但系数缺省」） | 0 | 0 | 45.8 px | 14.4 px |
| AP 按 `ap_order=5`（不迭代） | 0 | 0 | 0.032 px | 2.31e-4 px |
| AP@order5 且 `AP_0_0=0`（不迭代） | 0 | 0 | 0.032 px | 2.38e-4 px |
| **不动点迭代（64 轮）** | **2309** | **1085** | ~0 | ~0 |
| **AP/BP 作初值 + 迭代（候选修复）** | **2309** | **1085** | ~0 | ~0 |

结论：**唯一充分的条件是「先精化再判门」**；单改 `AP_0_0`、单改阶数、甚至把 AP/BP 全置零都不够（`AP_0_0` 让正中心越门，阶数截断/全零让离中心大片越门）。独立核对：Astropy/WCSLIB 自己迭代达成的残差是 `3.9e-7 px`，**本身就大于 `1e-7` 门**（`50-astropy-crosscheck.json`）。

### 7.3 根因判定

- **(a) 坐标/单位/像素基准错误？否。** 逐字段核对一致（§4）；独立 Astropy 以 `origin=0` 复现产品 `samples[]` 到 1e-10″（§6）。
- **(b) 几何/投影错误？否。** 前向（TAN+SIP A/B）被真实星 `rms≈0.13″`（生产解算）与独立 oracle 双重确认；`skyToPixel` 的不动点分支自身也能精确反演（残差 ~1e-13）。
- **(c) 消费者有效性门禁误杀可用逆变换？是。** `NaN` 全部来自 `wcs_transform.cpp:260-271` 的 `1e-7` 残差门；门被加在**未精化**的一次式结果上。

**引入点**：`38eaeba8b`（`fix(wcs): enforce TAN hemisphere, invertible CD and verified SIP inverse (#56 #57 #61)`，2026-10-16）。该 commit 为 #56 的一次式逆加入「必须验证前向残差」的门是对的，但**同时把门套到了显式 AP/BP 分支**，而 AP/BP 是 solver 用多项式拟合出来的近似逆（且消费者还把阶数截到 `sip_order`），因此该分支在真实数据上**必然**越门。

### 7.3bis 独立对抗性复核（另一 Agent，只读 /tmp，独立方法）

由一名独立执行者用**不共享本报告**的方法（自行编译真实源码 + 注入打印 + 改写 `ap_order` + Astropy oracle）复核，结论一致：

- **判定 (c)**：消费者侧门禁误杀可用逆变换；`cosc` 在所有 64 个真实样本与 CRVAL 处为 `0.999911…1.0`（#57 不触发）。
- **首个错误行** `wcs_transform.cpp:264` 的 `if`（比较在 `:265-266`，NaN 写于 `:267`）；触发式同上。
- **切点最决定性的反例**：`(ra,dec)=CRVAL` 时逆变换平凡地应为 `(2047.5,2047.5)`，却因 `AP(0,0)=(−4.33481e-05,−5.58219e-06)` 越门而返回 NaN；**全场最小残差 = 4.33e-5 px，是门的 433 倍**。
- 前向 `pixelToSky(sample.x,sample.y)` vs `samples[].ra/dec`：max **5.10e-11 arcsec**；Astropy 逆 max **3.9e-7 px**（**WCSLIB 自身残差已大于 1e-7 门**）。
- 反事实：`ap_order=5` 仍 64/64 NaN；`AP/BP=nullptr` 回退 0/64 NaN 且逆误差 7.09e-11 px；父版本 `38eaeba8b^` 的 AP 分支 64/64 有限（逆误差 0.7315 px，**无残差门**）。
- 额外确认：`a[i*6+j]` 布局正确（转置布局差 33″）；`P1SipCoeffs` 固定数组 ⇒ AP 分支恒被选中、迭代回退不可达。
- 独立核对的数值与本报告 §5–§6 完全吻合。

### 7.4 次级观察（未纳入本故障因果链，建议另立 Issue）

1. **产品 `roundtrip_px` 字段不是逆变换残差**。`module_adapters.cpp:4993` 用 `acsd::phase1::WcsTan::sky2pix`（**纯线性 TAN，无 SIP**，`wcs_tan.h:17-19`）对「含 SIP 的前向 ra/dec」求逆，于是 `roundtrip_px ≡ |SIP 像素位移|`。
   - 实测核对：帧1 角落 `(0,0)` 的 SIP 位移 `hypot(18.69, 18.87)=26.56 px`，与产品 `max_roundtrip_px=26.557`、`max_sip_effect_deg=0.00707°`（=26.55 px @0.9587″/px）完全一致。任何消费者把该字段当作「逆解精度」都会误判。
2. **本 main 的 `samples[].ra/dec` 确为含 SIP 的完整前向**（`sample_geometry` 自述 + `max_forward_cross_deg=2.8e-14` 交叉门 + 本次独立 Astropy 复现 ≤1.2e-10″）。此前「ipv 路径 samples 不含 SIP」的草案结论在本 HEAD 上**不成立**；被污染的只是 `roundtrip_px` 这一诊断字段。
3. **入帧数合理性交叉核对**：锥半径 0.9255°（`fov_radius_deg = √det·√(W²+H²)/2·1.2`）对应锥面积 ≈2.69 deg²，而 4096² @0.9587″/px 的帧面积 ≈1.18 deg² ⇒ 期望入帧比例 ≈44% ⇒ 2309×0.44≈1016；实测 1085。与「投影本身没问题、只是被门杀空」一致。

---

## 8. 与 #56/#57/#61 的关系（不重复施工）

- 三者都在 `38eaeba8b` 内落地，且**只改 `wcs_transform.cpp`**；本批未改任何生产文件。
- #57（TAN 背半球 `cosc≤0 → NaN`）、#61（`|det(CD)|<1e-15 → NaN`）在本故障中**不触发**（星都在切点半球正前方、`det=7.09e-8` 非退化）。
- #56 的诉求是「固定三轮迭代无残差检查，合法畸变可错位 3.38 px」。本批发现的**是 #56 修复的副作用**：门对「无 AP/BP 迭代分支」有效且必要，对「显式 AP/BP 一次分支」不可满足。**不需要重做 #56 的迭代/门逻辑，只需修正门与 AP/BP 分支的组合方式**。
- 因此本 Issue 与 #56 是**同一文件的不同实体缺陷**，不重复修 #56 已修内容（ hemispheres / singular CD / 迭代轮数上限均保持）。

---

## 9. 可执行修复方案（本批**未实施**，待明确授权）

> 排期 A1 规定：初批以「定位 + 可复跑负例」为界；实施代码与相同真实六帧 E2E 需**另行授权**。以下为按现有证据可执行的方案，不含任何「关 SIP / 放宽容差凑匹配 / 伪造匹配星」。

**P0（必需，最小改动，1 个文件）** — `wcs_transform.cpp::skyToPixel`
1. 保留 AP/BP 分支，但**只用它作不动点迭代的初值**；随后无条件跑与无 AP/BP 分支同一段不动点精化（同一实现，不新增第二份逆解数学）。
   - 注意：`P1SipCoeffs` 是固定数组 ⇒ 生产路径上 AP/BP 指针恒非空，**不能指望「传 nullptr 才走回退」**；必须显式精化。
2. 精化后再过前向残差门。门保留（继续保护 #56 的病态畸变），但此时残差可达 ~1e-13，`1e-7 px` 门不再误杀。
   - 依据：§7.2 的 `iterative` / `seeded_iterative` 两行 2309/2309 通过、1085 入帧。
3. 若迭代不收敛/非有限：保留「一次式 AP/BP 结果 + 其自身残差 ≤ 文档化容差」的兜底，仍不满足则 NaN（fail-closed 语义不减弱）。
   - 备选（独立复核建议，非首选）：仅把容差放宽到拟合逆可达量级（`≥1e-3 px`，以 SIP 作用量为界）。该法必须同时补 P0b，否则 3 阶截断下残差高达 0.71 px，放宽到 1e-3 仍会拒绝；且它放过了 `AP_0_0` 的语义含糊。**首选仍是精化**（可让门恢复到严格、且保留 #56 保护）。

**P0b（应同时修，消除真实阶数丢失）** — 让 AP/BP 用生产者声明的 `ap_order`
- `P1SipCoeffs.ap_order` 已在 `module_adapters.cpp:4627` 解析，但没有传进 `pc_calibrate_*`；`WcsTransform` 只有单一 `m_sip_order`。
- 在 `frame_photometry_fit`/`pc_api`/`WcsTransform` 三处补上 `ap_order`（或至少在 `WcsTransform` 内分别保存前向/逆向阶数）。
- 影响量级：帧角落残差 `0.71 px → 1.6e-3 px`（§6/§7.2）。P0 完成后该项只影响初值质量，但属真实合同缺口，应登记修正。

**P1（文档/合同）**
- 在 SIP 消费合同里写明：`1e-7 px` 是**迭代收敛判据**，不是对一次性多项式逆的验收门；AP/BP 为可选初值，不保证自身是精确逆。

**P2（回归测试，避免复发）**
- 用本批入仓的真实 `p1_wcs.json` 夹具（`evidence/40-p1_wcs-frame1.json`）断言：`skyToPixel` 对真实天球输入返回有限值且入帧数落在合理区间（≈1085，不得为 0）。
- 另加合成判据：`AP_0_0≠0` 与 `ap_order>sip_order` 两种生产者形态下，`skyToPixel` 仍精确；`det(CD)=0`、背半球、病态畸变仍必须 NaN。

**验收（下一授权批次）**
- 同真实六帧 CLI：`gaia_projected_in_frame` 由 0 恢复为正；`n_matched>0`；星匹配不再因投影为空而 `PHOT_FIT_NO_SCALE`。
- **本批不对「光度标度科学正确性」下结论**：进入 F_syn/IRLS 之后的 `k_photo`、应用标度、BUNIT/方差传播属安排 A2/#9 的独立验收。

---

## 10. PASS / FAIL / NOT_RUN

| 项 | 状态 |
| --- | --- |
| 冻结代码/二进制/真实输入哈希 | **PASS**（`00-identity.json`；Gaia 全量 sha256 = NOT_RUN，已记清单） |
| 生产构建（`ninja -C build/linux-sci-p5 acsd`） | **PASS**（exit 0） |
| 真实 CLI 复现 `0/2309 + PHOT_FIT_NO_SCALE + exit 4`（六帧） | **FAIL（复现成功）** |
| 生产调用链定位到 `pc::WcsTransform` | **PASS** |
| 逐字段生产者/消费者对照 | **PASS**（数值在打印精度内一致；`ap_order` 丢失已登记） |
| 真实 Gaia 2309 星投影探针（post=0/2309 NaN） | **PASS（负例）** |
| post/pre 单文件 A/B（0 ↔ 1085） | **PASS** |
| 独立 WCSLIB/Astropy 前向 oracle | **PASS**（≤1.2e-10″） |
| 独立 oracle 逆向可达 | **PASS**（≤3.9e-7 px） |
| 端到端交叉核对（Astropy 入帧 1085 == C++ 不动点 1085） | **PASS** |
| 独立对抗性复核（另一 Agent，独立方法，判定一致） | **PASS** |
| 首个错误位置 + 根因唯一判定 | **PASS**（`wcs_transform.cpp:265-266`，判定 (c)） |
| 修复实施 + 相同真实六帧 E2E | **NOT_RUN**（需另行授权，A1 边界） |
| 光度标度 `k_photo` / 应用标度科学正确性 | **NOT_RUN** |

---

## 11. 复跑命令

```bash
# 0) 冻结身份（含输入哈希）
python3 实验/issue92-wcs-gaia-match/code/00_freeze.py

# 1) 真实 CLI 复现（用 config 副本，输出到新目录；不覆盖 #12 证据）
build/linux-sci-p5/acsd normalize --json /workspace/issue92-scratch/cfg6_issue92.json -y   # exit 4

# 2) 投影探针 post/pre 单文件 A/B（真实 Gaia 2309 星）
python3 实验/issue92-wcs-gaia-match/code/run_wcs_probe.py \
  --p1wcs "/workspace/issue12-scratch/norm6_pre.p1-work/M42_M1_T3_flying_dutchman-20251126_041940-300S-Red/p1_wcs.json" \
  --out 实验/issue92-wcs-gaia-match/evidence --tag frame1

# 3) 独立 Astropy/WCSLIB oracle
python3 实验/issue92-wcs-gaia-match/code/wcs_astropy_oracle.py \
  实验/issue92-wcs-gaia-match/evidence/40-p1_wcs-frame1.json \
  --out 实验/issue92-wcs-gaia-match/evidence/10-astropy-oracle-20251126_041940-300S-Red.json

# 4) 逐字段对照 + 2309 星上的变量量化
python3 实验/issue92-wcs-gaia-match/code/30_field_compare.py \
  --p1wcs /workspace/issue92-scratch/norm6_issue92.p1-work/M42_M1_T3_flying_dutchman-20251126_041940-300S-Red/p1_wcs.json \
  --stderr /workspace/issue92-scratch/norm6_issue92.stderr \
  --out 实验/issue92-wcs-gaia-match/evidence/30-field-compare.json
python3 实验/issue92-wcs-gaia-match/code/20_project_variants.py \
  --p1wcs 实验/issue92-wcs-gaia-match/evidence/40-p1_wcs-frame1.json \
  --stars 实验/issue92-wcs-gaia-match/evidence/40-gaia-stars-frame1.csv \
  --out 实验/issue92-wcs-gaia-match/evidence/20-project-variants.json

# 5) 端到端独立交叉核对（Astropy 入帧数 vs C++ 不动点入帧数）
python3 实验/issue92-wcs-gaia-match/code/50_astropy_crosscheck.py \
  --p1wcs 实验/issue92-wcs-gaia-match/evidence/40-p1_wcs-frame1.json \
  --stars 实验/issue92-wcs-gaia-match/evidence/40-gaia-stars-frame1.csv \
  --out 实验/issue92-wcs-gaia-match/evidence/50-astropy-crosscheck.json
```

---

## 12. 交付边界

- 新增：`实验/issue92-wcs-gaia-match/{code,evidence}/**`、本报告。
- **未改**任何 `lib/**`、`docs/**`、`CMakeLists.txt`；未建分支、未开 PR；无旧格式兼容改动。
- 真实 FITS 只读、未上传；入仓为派生 JSON/CSV（含 2309 行真实 Gaia 星坐标）与可复跑脚本。
- 未用「关闭 SIP / 放宽匹配半径 / 伪造匹配星」等任何手段制造 PASS。
