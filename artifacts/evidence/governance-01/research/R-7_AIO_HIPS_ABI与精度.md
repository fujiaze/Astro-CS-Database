# R-7 · AIO/HiPS 的 ABI 与数值精度语义（研究线交付 · 决策建议书）

> **研究 Agent 交付物**。研究（非整改）任务：**零 git 写、未修改仓库任何文件**；全部产物落 `reports/PROJECT-GOVERNANCE-01/research/` 与 `run/PROJECT-GOVERNANCE-01/R-7/`。
> 依据 `工程控制/PROJECT-GOVERNANCE-01/RESEARCH_TASKS.md`（研究线 6 节交付要求）与 `OWNER_DECISIONS.md`。
> **不采信账本 `fix_state/verified_state`；以下每一条均由本轮在当树复跑/实测。**
>
> - 取证时点：`HEAD = e9b31285af6dd6c9bb2636f9e4d707a433f8abd7`（`fix(aio): AIO-001 nside 合法域校验 + properties 原子落盘（P0 M2b-A-01 闭合）+ 归属检查器升级`，2026-09-16 19:53:15 +0800），工作区含未提交改动（`git status --porcelain` 非空）。
> - **重要**：AIO-001 报告里的行号锚已随该提交漂移（同一 token 从 `:1069` → `:1106`、`:986` → `:1023`、`:1501` → `:1538`、`:949` → `:951`）。本报告**全部行号为本轮实测**，AIO-001 原锚另注。
> - **锚点复核与漂移声明**：`ENGINEERING_SPEC.md` 在本研究期间被前台**实时编辑**（同一 token 的行号从 `:8/:38/:128` → `:8/:41/:131` → `:8/:41/:137`），故对该文件的引用请**以 token 为准**；其余文件锚在复核时点全部命中。报告内**全部 `文件:行` 锚已用脚本逐条自校验**（`run/PROJECT-GOVERNANCE-01/R-7/verify_anchors.py` + `resolve_anchors.py`，日志 `logs/anchor_selfcheck.log`、`logs/anchor_resolve.log`；77 条锚，0 条未命中）。工作树在本研究期间被前台并发推进（`2f1dc334` / `ecdefb9f`，且 `ENGINEERING_SPEC.md` 在 `332149c0` 后行号整体 +3），故本报告锚点以**复核时点**为准。
> - 研究方法：① 抓取 IVOA HiPS 1.0 Recommendation 原文 PDF 并抽文；② 抓取 12 个**真实发布的 CDS HiPS FITS tile** 读其头卡；③ **链接仓库真实静态库**（`build/libastrocs_hips.a` + `build/libastrocs_aio.a` + `build/libastrocs_cfitsio.a`）写独立 C++ 探针驱动**生产 writer**；④ NumPy 独立复算；⑤ 逐条复跑仓库既有门。

---

## 0. 结论摘要（一页表）

| # | 议题 | 唯一推荐 | 置信度 | AIO-001 原判是否需要改 | 是否需要变更 claim |
|---|---|---|---|---|---|
| 1 | **M2b-A-02** HiPS leaf NSIDE 三源分歧 | **判「文档勘误」**：`docs/algorithms/HIPS_WRITER.md:146` 的 `NSIDE=2^k` 改为 `NSIDE=2^(k+9)`。写侧/读侧**不动** | **0.97** | **要改归类**：从「需权威裁决」降为「纯勘误，可直接派整改」 | 否 |
| 2 | **V11-N-01** `AioHipsSnrPoint` ABI | **方案 (c′) 原位版本化**：4 个跨边界结构首部加 `struct_size`+`abi_version` + C 边界 fail-closed 校验 + 单一权威 Python 镜像 + `aio_abi_layout_lock` 逐字段锁（照 `ipv_*` 样板）。**不选 (a)、(b)** | **0.90** | 判据成立，但「破坏 ABI 不可承受」的前提**经实测为假** | 否（新增 ABI 版本号 `=1` 属实现登记） |
| 3 | **M2b-H-01** `moc_sky_fraction` 字面精度 | **改精度、不改容差**：properties 与 manifest **共用同一格式化函数**，取 **`%.17g`**（round-trip 精确）。`<1e-9` 容差**保留** | **0.85** | 判据成立；但「既有断言把 6 位钉成期望」**必须**一并改，且现门**恒绿无判别力** | 否（容差不动） |
| 4 | **M2a-H-3** support 钳制后层级归约 | **改归约式**：层级累加用**未钳制的真实覆盖面积**（`flux_n += sig·a`、`area_n += a`），`sup` 只在**发布时钳一次**；并新增可观测的钳制计数。I2（support≤1）**保留** | **0.80** | 判据方向成立，但**归因需订正**：均匀覆盖下的低估**不是归约式的错**（生产在该域已达理论上限），错在**异质覆盖下的 sup 加权** | 是（`DATA_SEMANTICS §20.3` 的「F=signal×support×A_cell 闭合」需补「覆盖>1 时不可复原」的编码限声明） |
| 5 | **M9-F-3** `drizzle_scale_arcsec` 合法域 | **拒绝语义**：`!isfinite` → 2；`!(x>0)` → 2；`x > 824.52″` → 2；每次拒绝**必须 `set_error`**；删除「>0 才写」的静默省略分支 | **0.75** | 判据成立（NaN 被接受属实），上界**原判无值**，本报告给出推导值 | 否（新增实现常量 + 负面矩阵扩一列） |

**需负责人确认的那一句话**（详见 §5.6）：
> **请确认：把 `AioHipsSnrPoint` 等 4 个 AIO 跨边界结构的首部加上 `struct_size`/`abi_version`（即改变其二进制布局、使现行 40 字节版本不再兼容），并以 `ipv_*` 同款「C 探针 + 唯一 Python 镜像 + 逐字段布局锁」门锁死——实测仓内唯一的 Python 消费者 `hips_direct_smoke.py` 因硬编码已不存在的 DLL 路径而本就无法加载，故本次是零真实代价的破窗时机。**

---

## 1. 权威原文

> 定位：本节逐字抄录相关权威条款，并标注其在 `ASTROCS_DESIGN §0` 权威链中的位置。

### 1.0 权威链（`ASTROCS_DESIGN.md:7-32`）

```
① 本文 最高设计（ASTROCS_DESIGN）  →  ② AGENTS.md  →  ③ ENGINEERING_SPEC
  ④ CONTROL_PACK_SPEC  →  ⑤ docs/ci/  →  ⑥ docs/plugins/（23 篇）
  D -.公式引用.-> S(docs/science 权威)   D -.算法引用.-> AL(docs/algorithms 权威)
  D --> U(docs/design/UNIFIED_MODEL)
- 本文与其他任何文档冲突时，以本文为准。（:29）
```

- `ASTROCS_DESIGN.md:520-522`（附录 B）：**「IVOA HiPS 1.0、HEALPix（Górski 2005）、FITS WCS Paper I/II、FITS Standard、Drizzle（Fruchter & Hook 2002）—— 详见 docs/references/SCIENTIFIC_REFERENCES.md，具体 SCI claim 必须落实到论文节/式与项目推导差异。」** ⇒ 外部规范在权威链中作为**外部标准**被引用，非「参考」。
- `ASTROCS_DESIGN.md:400-404`（§7.3 模块与 DLL/SO 边界）：**「版本化 C ABI：跨 DLL 不传 STL/异常/RTTI/编译器私有类型；结构体带 `struct_size`/`abi_version`；所有权与释放方明确。」**
- `ENGINEERING_SPEC.md:8`：**「公共 C ABI 版本化；跨 DLL 不传 STL/异常/RTTI/编译器私有类型；」**
- `ENGINEERING_SPEC.md:41`：**「3. 公开头文件 —— 版本化 C ABI，带 `struct_size`/`abi_version`；」**
- `ENGINEERING_SPEC.md:137`：**「错误通过统一状态码+结构化诊断传播；不跨 C ABI 抛异常；」**

### 1.1 M2b-A-02 —— HiPS leaf / hierarchy tile 的 NSIDE

**（a）争议句（唯一异类）**
- `docs/algorithms/HIPS_WRITER.md:142`：`- (4c) 落盘：finalize 时对每阶 k<T（:797-880）：nside_k=2^(k+9)、`
- `docs/algorithms/HIPS_WRITER.md:146`：`  约定。FITS cards ORDERING=NESTED + NSIDE=2^k；三处 scatter 同 (2a) 式`

**（b）同文件上一句（自相矛盾）**
- `docs/algorithms/HIPS_WRITER.md:142-144`：`nside_k=2^(k+9)、A_cell_k=4π/(12·nside_k²)（:800-801）；cell 归一 signal=Σflux/Σarea、support=min(Σarea/A_cell_k,1)（:814-821）`
⇒ 同一 bullet 内先定义 `nside_k=2^(k+9)`，末尾却写 `NSIDE=2^k`。

**（c）同级/上级权威（全部指向 2^(k+9)）**
- `docs/contracts/DATA_SEMANTICS.md:327`：`| hierarchy 低阶 tiles（signal/support/variance/ivar 同目录树，nside=2^(k+9), k<tile_order） | 同上 [512×512] | 同上（父 cell=子像素聚合；f32 产品 float 累加 DISP-HIPS-009） | 空 acc 父 cell 照写全 NaN（DISP-HIPS-011） |`
- `docs/contracts/DATA_SEMANTICS.md:367`：`  :275）；头卡 NSIDE/FIRSTPIX="0"/LASTPIX="262143"（声明性，DISP-HIPS-012）。`
- `docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md:67`：`| hips_order | tile order K | 十进制整数，0 ≤ K ≤ 29；K 与 NSIDE=2^(K+9) 一致 |`
- `docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md:104`：`| NSIDE | 若存在必须等于 2^(K+9)（tile 像素分辨率） |`
- `docs/algorithms/HIPS_WRITER.md:86`（叶级 FITS 卡同一口径）：`OBJECT/FILTER/EXPTIME/DATE-OBS :197-204 + NSIDE/FIRSTPIX="0"/LASTPIX="262143"`
- `lib/infrastructure/aio/include/aio_hips.h:101`：`// nside - 叶级 NSIDE (2 的幂, >= 512)`

### 1.2 V11-N-01 —— `AioHipsSnrPoint` ABI

- `ASTROCS_DESIGN.md:403`：**「版本化 C ABI：…结构体带 `struct_size`/`abi_version`；所有权与释放方明确。」**
- `lib/infrastructure/aio/include/aio_hips.h:84-92`（逐字）：
```c
typedef struct {
    double ra_deg;
    double dec_deg;
    double snr;
    int64_t star_id;            // PSF 阶段 stable star_id (禁止重新编号)
    uint32_t quality_flags;     // 位标志: 1=PSF_OK 2=saturated 4=has_saturated
                                // 8=photo_matched 16=photo_rejected
    uint32_t photometric_status; // 0=unmatched 1=used 2=rejected
} AioHipsSnrPoint;
```
- `lib/infrastructure/aio/include/aio_hips.h:233-236`：`aio_hips_write_snr_points(AioHipsProductSet* ps, const AioHipsSnrPoint* pts, int n)`
- `ENGINEERING_SPEC.md:8/:41`（同上）。
- `eng/tools/check_abi_boundary.py:5`：`1. 所有 C ABI 结构(common_abi_v1.h 的 typedef struct)必须带 uint32_t struct_size + abi_version。`
- `eng/tools/quality/check_module_map.py:108`：`"header_missing_abi_version": "FAIL", "missing_implementation": "FAIL",`

### 1.3 M2b-H-01 —— `moc_sky_fraction` 字面精度与容差

- `docs/algorithms/HIPS_WRITER.md:297-300`（§9 冻结容差，逐字）：
```
- **冻结容差**：f64 通路逐像素 bitwise（同序确定性）；f32 存储 rtol=1e-7
  （单次乘除舍入界）；hierarchy f64 累加对 oracle bitwise、f32 累加路径
  rtol=1e-6（f32 累加器漂移界，DISP-HIPS-009 修复前口径）；sky fraction
  绝对误差 <1e-9；MOC/properties 键值精确相等；checksum 字段由 CFITSIO
  重算一致。
```
- `docs/algorithms/HIPS_WRITER.md:288`（不变量 I6）：`I6 moc_sky_fraction·4π=moc_area_sr；I7`
- `docs/algorithms/HIPS_WRITER.md:279-283`（(5a)，`moc_sky_fraction = moc_area_sr/4π` 在 `:282`）：`moc_sky_fraction = moc_area_sr/4π（finalize :1028-1029）`
- `docs/contracts/DATA_SEMANTICS.md:349-350`：`moc_area_sr=Σ A_cell(K)…；hips/moc_sky_fraction=moc_area_sr/4π；`

### 1.4 M2a-H-3 —— support 钳制与层级归约

- `docs/contracts/DATA_SEMANTICS.md:324`：`| support/…fits | 同上 | 无量纲 [0,1]（covered_area/A_cell，>1 钳 1.0；A_cell=4π/(12·nside²)） | 无效像素 0.0 |`
- `docs/contracts/DATA_SEMANTICS.md:346-347`：`support=min(covered_area/A_cell,1)、variance=var_num_sum/covered_area²、ivar=1/variance；F=signal×support×A_cell 闭合（gate7 复检同式）。`
- `docs/contracts/DATA_SEMANTICS.md:1034-1037`（§20.3 唯一权威）：`- **signal/support 逆变换合同**（matrix 专项）: 加权积分输出 signal（SCI-INT §5）与 support，逆变换回 flux_sum = signal×area（area = support×A_cell；stage2.cpp:1227-1228 chunk 路径 / :1588-1589 CPU 二次积分路径）`
- `docs/algorithms/HIPS_WRITER.md:135-136`（(4b)）：`逐父 cell 确定性累加（:549-550）：flux_n += signal·support·A_cell（面亮度→通量还原）、area_n += support·A_cell`
- `docs/algorithms/HIPS_WRITER.md:286`（I2）：`I2 support=min(area/A_cell,1)≤1`
- `docs/algorithms/HIPS_WRITER.md:288-289`（I7）：`I6 moc_sky_fraction·4π=moc_area_sr；I7` / `hierarchy 逐阶聚合闭合（f64 域 bitwise；f32 路径按 §9 容差）；I8`；`docs/algorithms/HIPS_WRITER.md:291`（I11）：`句柄不可复用；I11 F=signal×support×A_cell 有限非负（gate7 同式）；`
- `docs/plugins/algorithms_phase1/08_drizzle.md §4`（原判据）：signal 单位/源-目标像素面积/pixfrac/归一必须统一。

### 1.5 M9-F-3 —— `drizzle_scale_arcsec` 合法域

- `docs/algorithms/HIPS_WRITER.md:184-185`（逐字）：`[prov_set 时] ASTROCS_DRIZZLE_PIXFRAC（%.6f）/ ASTROCS_DRIZZLE_SCALE_ARCSEC（%.4f，>0 才写）（:727-736；set_drizzle_provenance :1007-1016，pixfrac∈(0,1] rc=2、scale≥0 rc=2）`
- `docs/algorithms/HIPS_WRITER.md:293-296`（负面矩阵，逐字）：`**负面矩阵**：nside<512、tile_width≠512、非法 dtype、越位 flags、parent_ipix≥12·4^K、width≠512、var_num NULL、全无效 variance tile（−5）、FITS 路径不可写（−4/−5/−6/−7）、prov pixfrac>1（rc=2）、重复 finalize（−2）、SNR metadata.xml 不可写。`
  ⇒ **负面矩阵只列 pixfrac，一个 scale 的边界都没有。**
- `lib/infrastructure/aio/include/aio_hips.h:238-243`：`// （K_CORR_DOMAIN 选项 B）：设置 Drizzle provenance（pixfrac / 像素角尺度），finalize 时写入 properties（ASTROCS_DRIZZLE_PIXFRAC / ASTROCS_DRIZZLE_SCALE_ARCSEC）。Phase2 sampler 按帧读取以选择 control-ivar 的 k_corr 标定值。`
- `ENGINEERING_SPEC.md:5.1`（边界/NaN/Inf/极端参数/错误输入须有测试）；`ENGINEERING_SPEC.md:3`（红线）。
- **消费侧适用域**：`docs/algorithms/PHASE2_SAMPLER.md:191-193`：`{1.2112,1.3925,1.4980 | 2.3958,2.8971,3.2035}（300"/600" × …` / `[0.5,1.0]×[300,600]（:95-96）；provenance 缺失/无有效 pixfrac →`；`docs/algorithms/v6/phase2-surface/ALG-P2-SURF-UPM.md:83`：`可复跑标定落地前，只允许在**已声明适用域内**取 k_corr=1.4，**禁止外推/内插**`。

---

## 2. 外部依据

### 2.1 IVOA HiPS 1.0 规范原文（规范原文引用）

**取得方式**：`curl -sSL http://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf` → 2 924 499 字节 / 32 页（`file` 判为 `PDF document, version 1.5, 32 page(s)`）；用 `pypdf 6.19.0` 抽文（`run/PROJECT-GOVERNANCE-01/R-7/extract_pdf.py`，输出 `REC-HIPS-1.0-20170519.txt`，32 页 56 887 字符）。
文档标识来自 `https://www.ivoa.net/documents/HiPS/`：**「HiPS - Hierarchical Progressive Survey Version 1.0 / IVOA Recommendation 19 May 2017 / DOI 10.5479/ADS/bib/2017ivoa.spec.0519F」**（注意：PDF 内页头写「19th May 2017」，而 ivoa.net 索引页写「19 May 2017」，`/20170519/` 路径一致）。

**关键原文（`REC-HIPS-1.0-20170519.txt` 行号 = 抽文行号；PDF §4.2.1，p.10–11）**

1. 行 316-318（**「打包」语义**）：
> `To avoid tiles containing only one HEALPix pixel, HiPS image tile hierarchy is S orders less deep than the original HEALPix resampled data, packaging the 2^S x 2^S HEALPix cell values, as an array of pixels.`

2. 行 328-331（**tile 宽度 = shift order S = 9**）：
> `In practice: because of present capacities of most network infrastructure a tile size of 512x512 pixels (shift order: S = 9) is a good compromi se between the size of the tiles and the number of tiles required to map the original survey data.`

3. 行 350-355（**★ 决定性公式**，PDF 式 (5) 的正文）：
> `The choice of the deepest HiPS order depends on the required HiPS resolution. It may be chosen as the first HEALPix order under the original pixel angular resolution. This c hoice depends on the tile width according to this HEALPix function:`
> `Tile pixel angular size =~ sqrt( 4*PI / (12 x (tileWidth x 2order)2) )`

4. 行 358-372（**表 5 的数值**，用于独立验算）：
> `Deepest HiPS Order | Number of tiles | Tile angular size | Tile pixel angular size`
> `0 | 12 | 58.63° | 6.871'`
> `7 | 196608 | 27.48' | 3.221"`
> `9 | 3145728 | 6.871' | 805.2mas`

**我的独立验算**（NumPy，见 §2.5-E）：`sqrt(4π/(12·n²))` rad → arcsec，取 `n = tileWidth·2^order`：
- order 0，n=512 → **412.26″ = 6.871′** ⟵ 与表 5 逐位一致；
- order 7，n=65536 → **3.221″** ⟵ 一致；
- order 9，n=262144 → **0.8052″ = 805.2 mas** ⟵ 一致。

⇒ **规范把「tile 内像素网格的 HEALPix 分辨率」唯一地钉为 `nside = tileWidth × 2^order`**；`tileWidth=512=2^9` 时即 **`nside = 2^(order+9)`**。这正是写侧/读侧采用的 `2^(k+9)`。

**补充（规范性细节，用于 §4 门禁自审）**：
- 我对抽文全文做过 `grep -i NSIDE` → **0 命中**；`grep -i ORDERING` 只命中 行 456「All tiles must have the same columns and the same column ordering.」（讲 catalogue TSV 的列序，与 HEALPix 无关）。
- 规范 properties 表（行 666-667）只冻结：`hips_order R Deepest HiPS order – Format: positive integer` / `hips_tile_width  Tiles width in pixels – Format: positive integer – Default : 512`。
⇒ **IVOA HiPS 1.0 并未规定 tile FITS 头必须出现 `NSIDE`/`ORDERING` 卡**；`NSIDE` 在本仓是「HEALPix FITS 惯例 + 本仓 IO-002 合同」的声明性卡，其**取值**仍由上面的规范公式唯一确定。

### 2.2 CDS Hipsgen 参考实现（同口径的第二外部依据）

`https://aladin.cds.unistra.fr/hips/HipsgenReferenceManual.html`（`Hipsgen reference manual related to Hipsgen/Aladin v12.646`），本地 `run/PROJECT-GOVERNANCE-01/R-7/hipsgen.txt`，**行 922-929**：

> `PARAMETER  mapNside - HEALPix map NSIDE`
> `DESCRIPTION   Dedicated to the MAP action. Allows you to specify a particular NSIDE value (default 1024). For this resolution to be identical to the original HiPS it should correspond to the formula: nside = tileWidth x 2^order.`

⇒ 与规范公式互为独立确认。

### 2.3 真实发布 HiPS 的 FITS 头（对拍实测）

下载 12 个真实 tile（CDS `alasky` 服务，4 个 survey × 3 个 order），用 astropy 7.0.1 读头：

| survey（`hips_tile_format`） | URL 中的 order | 头中出现的关键卡 |
|---|---|---|
| `DSS/DSS2Merged`（`jpeg fits`） | Norder0/Npix0 | `ORDER=0`, `NPIX=0`, `CTYPE1='RA---HPX'`, `PV2_1=4`, `PV2_2=3`, `CRPIX1=1024.5`, `CD1_1=-0.02197265625` |
| 同上 | Norder2/Npix0 | `ORDER=2`, `NPIX=0`, HPX WCS |
| 同上 | Norder3/Npix0、Norder4/Npix0 | 仅 `SIMPLE/BITPIX/NAXIS/NAXIS1=512/NAXIS2=512/BLANK`（被 Hipsgen `-trim` 过） |
| `2MASS/H`、`AKARI-FIS/N160`、`ACT/DR4DR6/f090` | Norder2 | `ORDER=<k>`, `NPIX=0`（ACT 为 `CTYPE1='GLON-HPX'`） |

实测输出（节选，完整见 §6 证据 12）：
```
https://alasky.cds.unistra.fr/2MASS/H/Norder3/Dir0/Npix0.fits   {'NAXIS1': 512, 'ORDER': 3, 'NPIX': 0, 'CTYPE1': 'RA---HPX', 'PV2_1': 4, 'PV2_2': 3}
```
- 12/12 个真实 tile **没有任何一个出现 `NSIDE` 卡**（`tiles with NSIDE card: 0`），也未见 `ORDERING`。
- 真实 tile 用 `ORDER=k` + `NPIX=n` 声明「本 tile 是 order-k 的第 n 个 cell」，像素分辨率由 `hips_order`×`hips_tile_width` 的规范公式推出。
- 12 个真实 tile 的 `NAXIS1 = 512` 全部一致 ⇒ 与 `hips_tile_width=512`、`S=9` 一致；真实 `properties`（`DSS/DSSColor`）实测：`hips_order = 9` / `hips_tile_width = 512` / `hips_version = 1.4` / `moc_sky_fraction = 1`。

⇒ **外部世界不存在「NSIDE=2^k」这一写法**；`2^k` 在规范/参考实现/真实产物里都无出处，只在本仓 `HIPS_WRITER.md:146` 一处出现。

### 2.4 ABI 兼容性最佳实践 + 仓库既有 ABI 门

**（a）仓库自带的「标准答案样板」（V2-N-01 / IpvParams 同型缺陷的已闭合修法）**
`问题扫描/findings/G_GOV_GATE/p1/V18.md:21` 自述：`ipv_abi_layout_lock_selfcheck`（Python 侧，且是**全仓唯一合格的负例自检形态**：`make_mutated_mirror()` 破坏真镜像后调**同一个** `check_all()` ⇒ **这就是 C-21 判据的标准答案样板**）`。其组成：

| 构件 | 路径 | 作用 |
|---|---|---|
| 唯一权威镜像 | `lib/algorithms/platesolve/tools/ipv_abi_mirror.py` | 头文件逐字段镜像；`_fields_` 首两项即 `struct_size`(offset 0)/`abi_version`(offset 4)；模块 docstring 明写「本文件是仓内**唯一**的 Python 侧镜像…不得各自复制一份 `_fields_`（V2-N-01 的根因正是镜像分叉）」 |
| C 探针 | `lib/algorithms/platesolve/cpp/ipv/test/ipv_abi_layout_probe.cpp` | `#include` 真头，打印 `sizeof/alignof` + 逐字段 `offsetof`/`sizeof` JSON |
| 布局锁 | `lib/algorithms/platesolve/cpp/ipv/test/ipv_abi_layout_lock.py` | 「字段名、字段顺序、字段 offset、字段 size、结构体 sizeof、alignof 任一不一致 => 退出码 1 (ctest FAIL)」+ `--selfcheck` 反向证明锁非恒真 |
| fail-closed 入口 | `lib/algorithms/platesolve/cpp/ipv/test/ipv_params_abi_failclosed_test.cpp` | 版本/尺寸不符时公共入口拒绝 |
| 注册面 | `eng/ci/checks.json:3374-3375`、`eng/ci/checks.json:3354-3355` | `ipv_abi_layout_lock` + `ipv_abi_layout_lock_selfcheck` 已进 CI |

**（b）通用 ABI 工程实践**（与 `ASTROCS_DESIGN §7.3` 同一族）：
- 跨边界结构首部放 `struct_size`：调用方先写自己编译期的 `sizeof`，被调用方 `if (s->struct_size < offsetof(...)+sizeof(field)) reject;` ⇒ 新旧双方在**运行期**互相识别，而不是靠构建系统约定；
- 对**数组元素**结构，仅加 `struct_size` 并不足以发现「调用方按更小步长分配」——必须由**入口显式接收元素步长/版本**，或对**每个元素**校验其首部；本报告的推荐同时满足两者（逐元素校验 + 结构自带 `struct_size`）；
- 镜像必须是**单点**：仓内任何 `ctypes` 副本都要 `import` 同一模块（`ipv_abi_mirror.py` docstring 的原文要求）。

### 2.5 我跑的实验

**A. 真实生产库 ABI 步长探针**（`run/PROJECT-GOVERNANCE-01/R-7/probe_abi.cpp`，链接 `build/libastrocs_hips.a + build/libastrocs_aio.a + build/libastrocs_cfitsio.a`）：
```
sizeof(AioHipsSnrPoint) = 40
offsetof ra_deg=0 dec_deg=8 snr=16 star_id=24 quality_flags=32 photometric_status=36
sizeof(PyMirror 4 字段镜像) = 32
步长比 = 40/32 ; 3 元素实际需 120 字节, 镜像只给 96 字节 (差 24)
输入(镜像语义): (10,20,1.5,1001) (30,40,2.5,1002) (50,60,3.5,1003) ; 缓冲区 96 字节
aio_hips_write_snr_points rc=0
aio_hips_finalize rc=0
```
写出的 `out/abi/snr/Norder0/Dir0/Npix4.tsv`：
```
# star_id ra dec snr quality_flags photometric_status
1001 10.000000000000 20.000000000000 1.5 0 1077805056
4632233691727265792 40.000000000000 2.500000000000 4.95053777e-321 0 1078853632
49 3.500000000000 0.000000000000 0 1569362382 5
```
⇒ 第 1 条正确，**第 2/3 条被 40 字节步长错位读**（第 3 条的 `snr=3.5` 落进了 C 的 `ra_deg` 槽），且第 3 条读取越过调用方 96 字节缓冲区（越界读 24 字节）。`rc=0`、`finalize rc=0` ⇒ **静默产出错数据**。

**B. 真实生产库层级通量探针**（`probe_m2ah3.cpp`，叶 `nside=1024`→`tile_order K=1`，父 `k=0`；mode1 = 8 个 32768 像素区块，`c = covered_area/A_cell` 分别为 0.25/0.5/1/1.25/1.5/2/4/8）：
```
R   c     sig   sup_pub  truth_flux    leaf_recon    parent_recon  叶损失%   父损失%   理想上限      可避免%
0   0.25  2.0   0.25     0.0163625     0.0163625     0.0163625     0.00     0.00     0.0163625    0.00
1   0.50  1.0   0.50     0.0163625     0.0163625     0.0163625     0.00     0.00     0.0163625    0.00
2   1.00  1.0   1.00     0.0327249     0.0327249     0.0327249     0.00     0.00     0.0327249    0.00
3   1.25  2.0   1.00     0.0818123     0.0654498     0.0654498    20.00    20.00     0.0654498    0.00
4   1.50  3.0   1.00     0.147262      0.0981748     0.0981748    33.33    33.33     0.0981748    0.00
5   2.00  4.0   1.00     0.261799      0.1309        0.1309       50.00    50.00     0.1309       0.00
6   4.00  5.0   1.00     0.654498      0.163625      0.163625     75.00    75.00     0.163625     0.00
7   8.00  6.0   1.00     1.5708        0.19635       0.19635      87.50    87.50     0.19635      0.00
```
mode2（同一父 cell 内 2 个 `c=4`/`sig=10` + 2 个 `c=1`/`sig=0.1`）：
```
每父 cell: truth_flux=80.2 A_leaf  area=10 A_leaf  真面积加权 sig=8.020000  (生产 sig_p=5.050000)
叶产物发布值对 (sig,sup): [(0.1, 1.0), (10.0, 1.0)]
父产物发布值对 (sig,sup): [(5.05, 1.0)]
truth_flux_total   = 5.25599e+06 A_leaf
leaf_recon_total   = 1.32383e+06 A_leaf   损失 74.813%
parent_recon_total = 1.32383e+06 A_leaf   损失 74.813% (vs truth) ; 理想上限 2.10239e+06 (可避免损失 37.032%)
```

**C. NumPy 独立复算 + 产物对拍**（`numpy_m2ah3.py`）：
```
[A] mode1 均匀: 产物父级 recon = 7.1994831645e-01 ; 由产物叶级 (sig,sup) 复算 = 7.1994831645e-01 ; 相对差 0.000e+00
    mode2 异质: 产物父级 recon = 1.3220868335e+00 ; 由产物叶级 (sig,sup) 复算 = 1.3220869086e+00 ; 相对差 5.680e-08
[B] c=1.25 → prod 损失 20.000% ; c=1.5 → 33.333% ; c=2 → 50.000% ; c=3 → 66.667% ; c=4 → 75.000% ; c=8 → 87.500% ; c=16 → 93.750%
    且 c>1 各行 fix_recon == prod_recon, 不可避% == prod损失% ⇒ 均匀覆盖下生产已达 support≤1 编码的理论上限
[C] (sig@c=4, sig@c=1)=(10,0.1): 真加权 sig 8.02, prod sig_p 5.05, fix sig_p 8.02, prod损失 74.813%
    (1000,0.001): 真加权 800, prod 500, fix 800, prod损失 75.000%
    (0.1,10): 真加权 2.08, prod 5.05 ⇒ 父级面亮度可**双向**偏（生产非面积加权）
[D] 20000 组随机 c∈(0,1]: prod 与 fix 的 sig_p 相对差最大 4.994e-16（纯浮点重结合，无语义差）
[E] 规范表 5 验算: sqrt(4π/(12·n²)), n=tileWidth·2^order → order0 412.26″=6.871′, order7 3.221″, order9 805.2 mas（与规范逐位一致）
```

**D. `std::to_string` 真实语义探针**（`moc_precision.cpp`，用真 libstdc++）：
```
K    N            N/(12*4^K)               to_string(6dp) %.8f           %.12f                      %.17g
0    1            0.083333333333333329     0.083333       0.08333333     0.083333333333             0.083333333333333329
0    3            0.25                     0.250000       0.25000000     0.250000000000             0.25
9    1000000      0.31789143880208331      0.317891       0.31789144     0.317891438802             0.31789143880208331
29   1            2.8912057932946783e-19   0.000000       0.00000000     0.000000000000             2.8912057932946783e-19
20   1000         7.5791225147744016e-11   0.000000       0.00000000     0.000000000076             7.5791225147744016e-11
```

**E. 收敛/精度律（`moc_precision.py`）**：
```
[A] 半量化步长 vs 冻结容差 <1e-9
   std::to_string (properties)            半量化步长=5.0e-07  余量=0.002 倍  **违反**
   %.8f (manifest.json)                   半量化步长=5.0e-09  余量=0.2   倍  **违反**
   %.9f (tests/io/make_hips_fixture.py)   半量化步长=5.0e-10  余量=2     倍  满足
   %.12f                                  半量化步长=5.0e-13  余量=2e+03 倍  满足
   %.17g (round-trip)                     半量化步长——0（精确回程）        满足
[B] 10 个真实 (K,N) 样本中违反 <1e-9 的个数: 6dp=7/10, 8dp=6/10, 9dp=0/10, 12dp=0/10
[D] 1/12 经 6dp: '0.083333' vs 0.083333333333333329 → 绝对误差 3.333e-07 = 容差的 333 倍
[F] 637 组随机真实覆盖样本中 6dp 与 8dp 字面量不同的占 637 组 (100.0%)
```

**F. `drizzle_scale_arcsec` 物理域推导**（`scale_domain.log`）：
```
k = 3600*180/pi*sqrt(pi/3) = 211076.28514206142
nside=512          pixel_scale = 412.258369 arcsec      (叶 nside 下界, M2b-A-01 已强制)
nside=536870912    pixel_scale = 0.000393 arcsec        (2^29 上界)
2x coarsest = 824.5167388361774 arcsec
```
公式来源：`DATA_SEMANTICS.md:349-350`：`hips_pixel_scale=3600·180/π·√(π/3)/nside arcsec（:704-705）`。

---

## 3. 实现事实

> 全部命令带 `timeout`；退出码与逐字输出见 §6。`FILE:LINE` 均为本轮当树实测。

### 3.1 M2b-A-02

| 源 | 命令 | 逐字输出 |
|---|---|---|
| 写侧·叶 | `sed -n '634p' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` | `cards.push_back({"NSIDE", std::to_string(ps->nside)});`（`ps->nside` 由 `begin` 收，语义 = 叶级 2^(K+9)） |
| 写侧·父 | `sed -n '1106,1109p' …` | `const uint32_t nside_k = 1u << (k + 9);` / `cards.push_back({"NSIDE", std::to_string(nside_k)});` |
| 读侧 | `sed -n '245p' runtime/io/hips_core.c` | `h->nside = 1ULL << ((uint64_t)order + 9u);` |
| 读侧校验 | `sed -n '632,640p' runtime/io/hips_core.c` | `if (strcmp(nm, "NSIDE") == 0) { … hips_set_err(err, cap, "tile NSIDE=%lld 与 order %d (nside=%llu) 不符", …)`  |
| 合同 | `grep -n 'NSIDE. \| 若存在必须等于' docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md` | `104:| \`NSIDE\` | 若存在必须等于 \`2^(K+9)\`（tile 像素分辨率） |` |
| 合同 | `grep -n 'nside=2\^(k+9)' docs/contracts/DATA_SEMANTICS.md` | `327:| hierarchy 低阶 tiles（… nside=2^(k+9), k<tile_order） …` |
| 测试 | `sed -n '226,243p' tests/io/test_hips_input_contract.py` | `# ---------- 布局不符: tile NSIDE 与 order 不符 ----------` / `# order1 期望 NSIDE=1024; 写成 2048 (order2 的 nside)` / `"NSIDE 与 order 不符应判 INVALID"` |
| 文档（唯一异类） | `grep -n 'NSIDE=2\^k' docs/algorithms/HIPS_WRITER.md` | `146:  约定。FITS cards ORDERING=NESTED + NSIDE=2^k；三处 scatter 同 (2a) 式` |

**行号漂移实测**：AIO-001 的 `02_M2b-A-02.log` 记录 `sed -n '1069p'` → 输出的是注释行；当树同 token 在 `:1106`（`grep -n 'k + 9\|k+9\|NSIDE\|leaf_order'` 命中 `1106: const uint32_t nside_k = 1u << (k + 9);`）。⇒ **AIO-001 的写侧锚已失效**，结论（三源分歧）仍成立但锚点需重取。

### 3.2 V11-N-01

| 事实 | 命令 | 逐字输出 |
|---|---|---|
| C 头字段 | `sed -n '84,92p' lib/infrastructure/aio/include/aio_hips.h` | 6 字段：`ra_deg/dec_deg/snr/star_id/quality_flags/photometric_status` |
| 无版本头 | `grep -n -e struct_size -e abi_version lib/infrastructure/aio/include/aio_hips.h` | **0 命中**（rc=1） |
| Python 镜像 | `sed -n '34,38p' lib/infrastructure/aio/tests/hips_direct_smoke.py` | `("ra_deg", c_double), ("dec_deg", c_double), ("snr", c_double), ("source_id", c_int64)` |
| 镜像旧路径（死代码） | `sed -n '14p;43p' 同文件` | `ROOT = r"F:\Astro dev\Astro CS Normalization Database"` / `aio = ctypes.CDLL(ROOT + r"\lib\astro_image_io\astro_image_io.dll")` |
| 路径是否还存在 | `ls lib/infrastructure/aio/astro_image_io.dll` | **存在**（但脚本拼的是 `lib\astro_image_io\astro_image_io.dll`，`lib/astro_image_io` 目录在 ARCH-001 迁移后**不存在**：`ls -d lib/astro_image_io` → 无此目录）⇒ 脚本不可加载 |
| 生产写入点 | `grep -n 'snr.push_back' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` | `951:            ps->snr.push_back(pts[i]);`（AIO-001 记 `:914/`:949`） |
| 实测破坏 | 见 §2.5-A | `sizeof=40 vs 32`；TSV 第 2/3 条错位 |

### 3.3 M2b-H-01

| 侧 | `FILE:LINE` | 逐字 |
|---|---|---|
| properties（image 面） | `aio_hips_writer.cpp:1023` | `kv.push_back({"moc_sky_fraction", std::to_string(moc_frac)});` |
| properties（SNR 面） | `aio_hips_writer.cpp:1261-1263` | `kv2.push_back({"moc_sky_fraction", std::to_string((double)cells.size() * 4.0 * kPi() / (12.0 * (1ULL << (2ULL * ps->tile_order))) / (4.0 * kPi()))});` |
| manifest.json | `aio_hips_writer.cpp:1538` | `"  \"moc_sky_fraction\": %.8f,\n"` |
| 测试夹具（第三套） | `tests/io/make_hips_fixture.py:171` | `("moc_sky_fraction", f"{len(idx) / npix:.9f}"),` |
| 既有断言（钉 6 位） | `lib/infrastructure/aio/tests/p1hips/p1hips_tests_units.cpp:292` | `P1HIPS_CHECK(cs, it != kv.end() && it->second == std::to_string(frac), "u1_prop_mocfrac");` |
| 既有断言（钉 6 位） | `p1hips_tests_properties.cpp:230` | `P1HIPS_CHECK(cs, it != kv.end() && it->second == std::to_string(3.0 / 12.0), "i6_sky_fraction");` |
| 既有 oracle（1e-9） | `p1hips_tests_oracle.cpp:124-130` | `// sky fraction 弱上界: 3/12 cells → |frac−0.25| < 1e-9 (§9 冻结)` / `P1HIPS_CHECK(cs, std::fabs(got - 3.0 / 12.0) < 1e-9, "o2_skyfrac_1e9");` |

**三源字面量并存实测**：同一产品同名键，properties 6 位、manifest 8 位、夹具 9 位；637 组随机真实覆盖样本中 **100%** 的样本 6 位与 8 位字面量不同（§2.5-E[F]）。

### 3.4 M2a-H-3

| 位置 | `FILE:LINE` | 逐字 |
|---|---|---|
| 叶级归一 + 钳制 | `aio_hips_writer.cpp:605-609` | `double sig = 0.0, sup = 0.0;` / `if (v && area > 0.0 && std::isfinite(flux) && std::isfinite(area)) {` / `sig = flux / area;` / `sup = area / ps->A_cell;` / `if (sup > 1.0) sup = 1.0;` |
| 叶级缓存 | `aio_hips_writer.cpp:619-621` | `sigF[fi] = (float)sig; supF[fi] = (float)sup;` / `sig_n[i] = (double)(float)sig; sup_n[i] = (double)(float)sup;` |
| **层级归约（钳后反乘）** | `aio_hips_writer.cpp:679-680` | `flux = sig_n[i] * sup_n[i] * ps->A_cell;` / `area = sup_n[i] * ps->A_cell;` |
| 累加器 | `aio_hips_writer.cpp:403-407` | `void add(size_t i, double flux, double area) {` … `sumFluxD[i] += flux; sumAreaD[i] += area; ++count[i];` |
| 父级归一（再钳一次） | `aio_hips_writer.cpp:1120-1124` | `sig = flux / area;` / `sup = area / A_cell_k;` / `if (sup > 1.0) sup = 1.0;` |
| 钳制不可观测 | `grep -c "support_clamped" aio_hips_writer.cpp` | `0`（rc=1） |
| 生产上游 | `astro_sphere_sink.cpp:124-125` | `dense_area[local] = acc.sumArea;` / `view.covered_area = dense_area.data();`（直写面：**未预先钳制**，故 `sumArea>A_cell` 可到 writer） |
| 生产上游（Phase1 面） | `astro_sphere_sink.cpp:286-290` | `double S = (double)acc.sumArea / a_cell; if (S > 1.0) S = 1.0; long q = std::lround(255.0 * S); … area_buf[local] = (float)(((double)q / 255.0) * a_cell);`（Phase1 面在 sink 就钳到 A_cell） |

**父子聚合比实测**（不是 1:1）：叶 `nside=1024`（order 10）、`tile_order K=1`、父 `k=0`（order 9）下，`z = ((s<<18)|i) >> 2`，每个父 cell 由 **4** 个叶像素累加（`count` 数组可证；生产测试 `p1hips_tests_properties.cpp:95-84` 的 oracle 亦按 4 个叶像素求和）。父 `A_cell_k = 4·A_cell`（实测 `A_K0/A_LEAF = 4.000000`）。

### 3.5 M9-F-3

| 位置 | `FILE:LINE` | 逐字 |
|---|---|---|
| 参数校验 | `aio_hips_writer.cpp:1318-1319` | `if (!(pixfrac > 0.0 && pixfrac <= 1.0)) return 2;` / `if (scale_arcsec < 0.0) return 2;` |
| **无 set_error** | `sed -n '1312,1326p'` | 两处 `return 2;` 前**均无 `set_error(...)`**（对比同文件其它入口如 `:558-561` 都带 `set_error`） |
| 序列化 | `aio_hips_writer.cpp:1012-1015` | `std::snprintf(sc, sizeof(sc), "%.4f", ps->drizzle_scale_arcsec);` / `kv.push_back({"ASTROCS_DRIZZLE_SCALE_ARCSEC", sc});` |
| **静默省略** | `aio_hips_writer.cpp:1012` | `if (ps->drizzle_scale_arcsec > 0.0) {`（scale=0 时 setter 返 0 成功，但键不落盘） |
| 既有负例（部分） | `lib/infrastructure/aio/tests/p1hips/p1hips_tests_negative.cpp:269-274` | `P1HIPS_CHECK_EQ(cs, aio_hips_set_drizzle_provenance(nullptr, 0.5, 1.0), 1);` / `(ps, 1.5, 1.0), 2` / `(ps, 0.0, 1.0), 2` / `(ps, 0.5, -1.0), 2` / `(ps, 0.5, 0.1), 0` |
| 既有正例 | `p1hips_tests_properties.cpp:344-345` | `kv1.at("ASTROCS_DRIZZLE_SCALE_ARCSEC") == "0.3500"` |

**NaN 被接受的形式推导**：`scale_arcsec = NaN` ⇒ `NaN < 0.0` 为 **false** ⇒ 不返回 2 ⇒ 走到 `ps->drizzle_prov_set = true; ps->drizzle_scale_arcsec = NaN;` ⇒ `finalize` 时 `NaN > 0.0` 为 false ⇒ **键不落盘**、但 `ASTROCS_DRIZZLE_PIXFRAC` 已落盘 ⇒ 产出**半套 provenance**。`+Inf` 同理通过校验，且 `Inf > 0` 为 true ⇒ `%.4f` 写出 `inf`。

---

## 4. ⭐ 门禁自审（每条门/判据本身是否正当）

### 4.1 M2b-A-02 的门

| 维度 | 评判 |
|---|---|
| ① 有无权威依据 | **有**。外部：IVOA HiPS 1.0 REC §4.2.1 公式 + Hipsgen 手册 `nside = tileWidth × 2^order` + 12/12 真实 CDS tile 的 `ORDER/NPIX/HPX WCS` 形态。内部：`DATA_SEMANTICS:327`、`IO_002:67/:104`、`HIPS_WRITER.md:86/:145`、`tests/io/test_hips_input_contract.py:232`。 |
| ② 可测量、可复现 | **可**：`grep -n 'NSIDE=2\^k' docs/algorithms/HIPS_WRITER.md` 一条命令即可判定；写/读两侧的取值同一命令可读。 |
| ③ 是否把实现缺陷误判成文档错误 / 反之 | **AIO-001 判「偏差在文档」——正确**（本报告独立复验一致）。但要**换归类**：这不是「口径分歧需裁决」，而是**同段自相矛盾的勘误**（`HIPS_WRITER.md:142` 已定义 `nside_k=2^(k+9)`，`:146` 却写 `2^k`）。把纯勘误上呈裁决，会让负责人承担一次本不需要的科学判断。 |
| ④ 阈值/量测域是否定义清楚 | 清楚且**唯一**：`tileWidth=512`（`IO_002:36-40` 校验 `hips_tile_width` 必须为 2 的幂，512 为标准；writer `:558` 强制 `width==512`），`S=9`。 |
| ⑤ 误报/漏报 | **漏报面**：全仓**没有任何机器门**覆盖「`docs/algorithms/**` 的公式/卡值是否正确」。`docs/algorithms` 是权威域却不设门，只能靠人工复检 ⇒ 这是本条的真正系统性缺口。**另**：`HIPS_WRITER.md` 内部所有 `:NNN` 源码锚均已相对当树漂移（`:797-880` → `:1097-1200`；`:800-801` → `:1107-1108`；`:814-821` → `:1119-1126`），属**同一文档的第二类勘误**，建议同批处理。 |
| **门应为** | 「`ALG-*` 文档里的冻结常量/卡值必须与 `contracts/**` 或实现**逐 token 一致**」的机器检查（至少覆盖 `NSIDE=`、`A_cell=`、`nside_k=` 这类可正则化的等式）。当前形态下此类矛盾只能靠人工发现。 |

### 4.2 V11-N-01 的门

| 维度 | 评判 |
|---|---|
| ① 权威依据 | **有且明确**：`ASTROCS_DESIGN §7.3:403`、`ENGINEERING_SPEC:8/:41`。 |
| ② 可测量、可复现 | 本报告用 C 探针 + 真实 TSV 产物做到了**端到端可复现**（§2.5-A）。 |
| ③ 误判方向 | AIO-001 判「实现缺陷」——**正确**。但它把「改动会破坏已冻结 ABI」当作**阻塞理由**；本报告实测：`hips_direct_smoke.py:43` 硬编码的 `lib\astro_image_io\astro_image_io.dll` 在 ARCH-001 迁移后**不存在**（`lib/astro_image_io` 目录已无），即**唯一 Python 消费者本就不可加载**；C++ 侧唯一调用点 `astro_sphere_sink.cpp` 与 writer 同仓同版本。⇒ 「破坏面不可承受」的前提**不成立**。 |
| ④ 阈值/量测域 | 可精确测量：`sizeof`/`offsetof` 逐字段 + 3 条 TSV 行。 |
| ⑤ 误报/漏报 | **两条既有 ABI 门都有覆盖漏洞**（本轮实测）： |
| | **（i）`eng/tools/check_abi_boundary.py` 的扫描面不含 aio**：`HEADERS = [REPO/"include/astrocs/common_abi_v1.h"]`（`:13-15`）+ `backend_host/*.h`（`:16-23`）。运行结果 `CPU-001_PASS: ABI 结构 11 个均带 size/version; 11 头无 STL/异常跨边界`（rc=0）——**全绿，而 `aio_hips.h` 的 4 个跨边界结构一个 token 都没有**。 |
| | **（ii）`eng/tools/quality/check_module_map.py` 被「任一命中」绕过**：`eng/tools/quality/check_module_map.py:353-357` 的判据是 `if not any(re.search(r"abi_version\|struct_size", p.read_text()) for p in headers)` —— 对**整个模块目录**做 `any()`。`lib/infrastructure/aio/include/aio_pipeline.h:84-85`（`:113` 再次出现）含 `uint32_t abi_version;` / `uint32_t struct_size;`，于是 `aio` 模块**不会**报 `header_missing_abi_version`，尽管 `aio_hips.h` 里的 `AioHipsSnrPoint`/`AstroSphereTileView`/`AioHipsDiagTileView`/`AioHipsTile` 全部缺失。实测该门对 aio 只输出 `aio NOT_IMPLEMENTED lib/infrastructure/aio missing_cmake_target missing_implementation missing_module_yaml product_unit_skeleton`（**无 header_missing_abi_version**）。且该门**恒返 rc=0**（本轮输出 133 条 FAIL 仍 rc=0）⇒ 判据是**咨询性**的。 |
| | **（iii）`ipv_abi_layout_lock` 存在但只锁 IpvParams**（`lib/algorithms/platesolve/cpp/ipv/test/`），aio 无对应目标。 |
| **门应为** | ① `check_abi_boundary.py` 的 `HEADERS` 扩到 `lib/infrastructure/aio/include/aio_hips.h`（或改为「逐 `typedef struct` 块」判定，而非全文 `in abi`）；② `check_module_map.py` 的 `any()` 改为「每个公开头必须自证」或「每个 `typedef struct` 块必须带 token」；③ 新增 `aio_abi_layout_lock`（C 探针 + 唯一镜像 + `--selfcheck` 变异负例），并**入 `eng/ci/checks.json`**。 |

### 4.3 M2b-H-01 的门

| 维度 | 评判 |
|---|---|
| ① 权威依据 | **弱**。「sky fraction 绝对误差 <1e-9」在 `HIPS_WRITER.md §9:296-300` 只有断言，**没有误差预算推导**（对比同段其它容差都写了来源：`rtol=1e-7（单次乘除舍入界）`、`rtol=1e-6（f32 累加器漂移界）`）。 |
| ② 可测量、可复现 | **不可**（在本轮实测的意义上）：唯一施加该阈值的断言 `p1hips_tests_oracle.cpp:130` 用 `3/12 = 0.25` 这一点——**任何 ≥1 位小数的格式在该点都精确**（§2.5-E[C]）⇒ 该门**对序列化精度零判别力，恒绿**。 |
| ③ 误判方向 | AIO-001 判「与冻结容差面不自洽」——**正确**；但它把问题表述为「统一到哪个字面精度会改变冻结容差面」，暗示可能要动容差。本报告结论相反：**容差可原样保留，必须动的是序列化**（见 §4.3⑤）。 |
| ④ 阈值/量测域 | **未定义清楚**：`<1e-9` 没有说明它约束的是「计算出的 double」还是「序列化后的字面量」。若是前者，序列化精度与之无关（那本条就不是缺陷）；若是后者，则 6 dp（半量化 5e-7）**超 500 倍**、8 dp（5e-9）**超 5 倍**、连测试夹具的 9 dp（5e-10）也只有 **2 倍**余量。同一句还写「MOC/properties 键值精确相等」，进一步把「字面量」拉进量测域。 |
| ⑤ 误报/漏报 | **漏报**：manifest 侧的 `%.8f` 完全不在任何容差断言的量测域内；`kv2`（SNR 面 properties）的 `std::to_string` 也无人管；`moc_sky_fraction` 的小值区（K≥20 时 6 dp 与 12 dp 都写 0）无人管。**误报风险**：若把 <1e-9 施加到 `1/12` 这个真实用例（同文件 `p1hips_tests_units.cpp:292` 就在用它），门立刻红 ⇒ 现门与现实现**不可能同真**。 |
| **门应为** | 冻结条款改写为可判定的三段式：**（1）唯一格式化函数**——properties 与 manifest **必须**由同一函数产出，且该函数是**唯一**写点（可用 `grep -c` 计数）；**（2）半量化律**——`0.5·10^(-p) ≤ tol/10`（tol=1e-9 ⇒ p ≥ 10；若用有效位格式则要求 `|parse(lit) − v| ≤ tol`）；**（3）非平凡测试点**——断言点必须取**不能被有限小数精确表示**的值（如 `N=1, K=0` 的 `1/12`），否则门无判别力。 |

### 4.4 M2a-H-3 的门

| 维度 | 评判 |
|---|---|
| ① 权威依据 | **半有**。`support ≤ 1`（I2/`DATA_SEMANTICS:324`）有外部依据（HiPS 的 support 是「覆盖比例」，天然 ≤1；且 IVOA §4.2.1.2 对低阶像素只说「mean/median/first pixel」，未规定 >1 覆盖的编码）。但「F=signal×support×A_cell 闭合」这条**不是外部标准**，是本仓自定的**同源**不变量。 |
| ② 可测量、可复现 | 本报告做到了（真实库探针 + NumPy，见 §2.5-B/C）。 |
| ③ 误判方向 | **AIO-001 的归因需要订正**（这是本条最重要的自审结论）：<br>• 均匀覆盖下（同一父 cell 内所有叶像素同 `c`），生产 `parent_recon` **等于**「support≤1 编码下的理论上限」，**可避免损失 = 0.00%**（§2.5-B 表第 9 列全 0）⇒ 那部分低估**不是归约式的错**，而是 `support≤1` 的**编码极限**（`c=1.25→20.00%`、`1.5→33.33%`、`2→50.00%`、`4→75.00%`、`8→87.50%`）。<br>• **真正的实现缺陷在异质覆盖**：生产用 `sup_j`（被钳制值）当权重，得到的是 **sup 加权均值**而非**面积加权均值**。实测 mode2（同父 cell 内 2×(c=4,sig=10) + 2×(c=1,sig=0.1)）：真面积加权 `sig=8.02`，生产 `sig_p=5.05`，父级通量损失 **74.813%**，其中相对理论上限的**可避免损失 37.032%**。<br>• 生产并非单向高估：`(sig@c=4, sig@c=1)=(0.1,10)` 时真加权 `2.08` 而生产 `5.05`（**高估**）⇒ `sig_p` 的偏差**双向**；但**通量** `recon=Σ_j sig_j·min(a_j,A_cell)` **严格 ≤ 真值**（数学上：`min(1,A_cell/a_j) ≤ 1`），「系统性低估」对**通量**成立。 |
| ④ 阈值/量测域 | 量测域未定义：`>1 钳 1.0` 没写「钳制后信息丢失如何处理」，也没写「层级归约用钳前还是钳后面积」。`A_cell` 到底指叶级还是父级在 `(4b)` 里有歧义（`area_n += support·A_cell` 用的是**叶级** `A_cell`，而 `(4c)` 又用 `A_cell_k`）。 |
| ⑤ 误报/漏报 | **门恒真**：`I7 hierarchy 闭合` 的 oracle（`p1hips_tests_properties.cpp:95`）**逐字复刻了生产公式** `flux += (sv * pv) * a_leaf; area += pv * a_leaf;` ⇒ 与实现同源，**不能**发现归约式本身的错误。这正是 `OWNER_DECISIONS.md F-5`（`M1a-F-001` 同源 Oracle 无区分力）已在别处认定的失败模式，在本条重演。 |
| **门应为** | ① I7/gate7 的 oracle 改为**独立**复算（父级 signal = `Σf_j/Σa_j`，权重用**未钳制面积**；support = `min(Σa_j/A_cell_k,1)`），并与实现**分离**到不同文件；② 冻结条款补一句**编码限声明**：「`support∈[0,1]` 使 `Σa_j > A_cell_k` 的父 cell **无法**从 `signal×support×A_cell` 复原全部通量，通量守恒的判据应为 `recon = min(Σf_j, (Σf_j/Σa_j)·A_cell_k)`」；③ 新增可观测计数（钳制像素数 / 覆盖>1 像素数）入 properties+manifest，使该条件**可测量**。 |

### 4.5 M9-F-3 的门

| 维度 | 评判 |
|---|---|
| ① 权威依据 | **无**。全仓对 `drizzle_scale_arcsec` 的合法域只有一句实现描述（`HIPS_WRITER.md:184-185`「`scale≥0 rc=2`」「`>0 才写`」），`HIPS_WRITER.md:293-296` 的负面矩阵**只列 pixfrac**。`grep -rn "drizzle_scale_arcsec" docs/ contracts/ eng/tools/ cli/` 只有 `HIPS_WRITER.md:184` **一处**命中。 |
| ② 可测量、可复现 | 可（读代码 + 一次调用即可），但**当前没有任何测试覆盖 0/NaN/Inf/上界**。 |
| ③ 误判方向 | AIO-001 判「无上界、无 isfinite、NaN 被接受」——**逐字成立**。但它把「上界取值」直接归为「科学域参数定义不可自决」；本报告给出**可从仓内已冻结常量推导**的上界（§5.5），因此该条**不必上呈裁决**。 |
| ④ 阈值/量测域 | **完全未定义**（既无上下界，也没说 0 是合法值还是错误）。 |
| ⑤ 误报/漏报 | **双重语义缺陷**：<br>• `scale=0` 时 setter **返回 0（成功）**但 `finalize` **不落键** ⇒ "调用方以为设置了 provenance，产品里没有" —— 这违反同族接口在 `aio_hips.h:184-185` 自述的「**通道策略 = 全或无**…参数不合法立即返回非 0（禁静默缺键/占位值）」。<br>• 两处 `return 2` **不带 `set_error`** ⇒ 违反 `ENGINEERING_SPEC:137`「错误通过统一状态码**+结构化诊断**传播」。<br>• `%.4f`（1e-4 量化）比本报告推荐的下界 `3.93e-4″` 只大 4 倍 ⇒ 在细尺度端相对误差可达 25%。 |
| **门应为** | 负面矩阵扩为「**参数 × 域**」二维表，`drizzle_scale_arcsec` 至少 5 行：`NaN`/`+Inf`/`0`/`负`/`>上界`，每行断言 `rc` 与 `last_error` 非空；并新增「`scale>0` 时键必须落盘」的正例（锁死静默省略）。 |

---

## 5. 建议裁决

### 5.1 M2b-A-02 —— 唯一推荐：判文档勘误，改 `2^k` → `2^(k+9)`

- **唯一推荐（0.97）**：`docs/algorithms/HIPS_WRITER.md:146` 的 `NSIDE=2^k` 改为 `NSIDE=2^(k+9)`。**写侧 `aio_hips_writer.cpp:1109`、读侧 `runtime/io/hips_core.c:245/:632` 一律不动**。
- **依据**：外部 —— IVOA HiPS 1.0 REC §4.2.1（`packaging the 2^S x 2^S HEALPix cell values` + `Tile pixel angular size =~ sqrt(4*PI/(12 x (tileWidth x 2^order)^2))`，且表 5 的三个数值我全部复算一致）+ Hipsgen 手册 `nside = tileWidth x 2^order` + 12/12 真实 CDS tile 均无 `NSIDE=2^k` 写法；内部 —— `DATA_SEMANTICS:327`、`IO_002:67/:104`、`HIPS_WRITER.md:86/:145`（同文件自相矛盾）、`test_hips_input_contract.py:232`。
- **反方（最可能被反驳的点）**：
  1. 「`NSIDE` 卡是仓自定声明，规范根本没规定它，所以 `2^k` 也能自圆其说」——**不成立**：真实 CDS tile 用 `ORDER=k`+`NPIX=n` 声明 tile 身份，而本仓把它命名为 `NSIDE`（HEALPix FITS 的"分辨率参数"，语义即**本文件像素网格的 nside**），若填 `2^k` 就与「文件里有 512×512 个 order-(k+9) 像素」直接冲突，且与读侧 `h->nside`（用于 tile 地址/面积推导）矛盾。
  2. 「也许写侧该改成 `2^k` 更好」——**不成立**：改成 `2^k` 会同时打破 `IO_002:104` 读端合同与其 3 条负例（`test_hips_input_contract.py:226-243`）、`DATA_SEMANTICS:327`、以及 `hips_core.c` 的一致性校验，且无任何外部依据支持。
- **最小改动路径**：`docs/algorithms/HIPS_WRITER.md` **单 token** 勘误（`docs/algorithms/**` 是"只读权威"域，勘误须由负责人/前台按文档集变更流程执行；本研究线不改）。**不需**变更 claim、**不需**改 registry、**不需**改代码。
- **影响面**：零代码影响；`HIPS_WRITER.md` 内部 `:NNN` 源码锚（`:797-880`→`:1097-1200` 等）建议同批刷新（同一文档、同类勘误）。
- **归类建议**：从 `AIO-001/P0_OPEN_CONCLUSIONS.md`「需权威裁决清单」第 1 条**移出**，转为可派整改任务。

### 5.2 V11-N-01 —— 唯一推荐：方案 (c′) 原位版本化 + 单一镜像 + 布局锁

- **唯一推荐（0.90）**：
  1. **结构体版本化（原位，首部）**：给 `lib/infrastructure/aio/include/aio_hips.h` 的 4 个跨边界结构 `AioHipsSnrPoint`、`AstroSphereTileView`、`AioHipsDiagTileView`、`AioHipsTile` 各加首部两字段 `uint32_t struct_size; uint32_t abi_version;`，并定义 `AIO_HIPS_SNR_ABI_VERSION = 1` 等常量（照 `ipv_api.h` 的 `IPV_PARAMS_ABI_VERSION`）。
  2. **C 边界 fail-closed**：`aio_hips_write_snr_points` / `write_signal_support_tile` / `write_variance_tile` / `write_diag_tile` / `aio_hips_write` 必须**逐元素**校验 `struct_size == sizeof(AioHipsSnrPoint)`（等）与 `abi_version`，不符即返回新错误码（建议 `-9`）+ `set_error`，**永不按盲步长读取**。这一条是硬要求：仅加首部而不校验，仍不能发现"调用方按 32 字节步长分配"。
  3. **单一权威 Python 镜像**：新建 `lib/infrastructure/aio/tools/aio_abi_mirror.py`（照 `ipv_abi_mirror.py`，docstring 写明"唯一镜像、禁止复制 `_fields_`"），删除 `hips_direct_smoke.py:34-38` 的内联镜像。
  4. **布局锁**：新建 `lib/infrastructure/aio/tests/abi/aio_abi_layout_probe.cpp`（只 include `aio_hips.h`，输出 `sizeof/alignof/offsetof` JSON）+ `aio_abi_layout_lock.py`（逐字段比对 + `--selfcheck` 变异负例），注册为 ctest 目标并写入 `eng/ci/checks.json`（照 `eng/ci/checks.json:3374-3375` 的形态）。
  5. **顺手关闭两个门禁漏洞**：`eng/tools/check_abi_boundary.py` 的 `HEADERS` 加入 `lib/infrastructure/aio/include/aio_hips.h`；`eng/tools/quality/check_module_map.py:353-357` 的 `any()`（`:355`）改为逐头/逐结构块判定。
  6. **订正 `hips_direct_smoke.py:14/:18/:43`** 的 `F:\Astro dev\...` 与 `lib\astro_image_io\astro_image_io.dll` 死路径（否则新镜像无法被任何东西验证）。
- **为什么不是 (a)（只补 Python 镜像 2 字段）**：补镜像**必须做**（否则现有唯一的 Python 消费者仍错），但它**不满足** `ASTROCS_DESIGN §7.3:403` 与 `ENGINEERING_SPEC:41` 的可检条款，也不会阻止同类缺陷再发生 —— `V2-N-01`（`IpvParams`）已经因为「头改了、镜像没改」撞过一次（`问题扫描/账本/FIX_LEDGER.md:97`）。把 (a) 当完整方案 = 明知有两个失守的 ABI 门却不修。
- **为什么不是 (b)（单独改 C 布局）**：同样是 ABI 破坏，却换不到任何自描述能力（没有 `struct_size` 就无法在运行期识别旧调用方），纯粹是"付了代价、没买到东西"。(b) 只在"必须保持 40 字节"时才有意义，而这一约束**没有依据**。
- **关于"破坏 `API-HIPS-001`"的反方论证及其反驳**：
  - 反方：`OWNER_DECISIONS.md` 与 `AIO-001` 都把「破坏已冻结的跨 DLL C ABI」当成不可自行决定的阻塞理由。
  - 反驳（本轮实测）：仓内**唯一**的 Python 消费者 `hips_direct_smoke.py:43` 指向 `ROOT + r"\lib\astro_image_io\astro_image_io.dll"`，而 `lib/astro_image_io` 目录在 ARCH-001 迁移后**已不存在**（现文件在 `lib/infrastructure/aio/astro_image_io.dll`）⇒ 该消费者**当前完全无法加载**，不存在"线上旧调用方"。C++ 侧唯一调用点是 `astro_sphere_sink.cpp`（`write_hips_direct` / `write_hips_phase1`），与 writer 同仓同版本、必须一起编译。⇒ **ABI 破坏面是理论面**。
  - 残余风险的诚实登记：若负责人坚持零破坏，退路是**新增 v2 入口并保留 v1**（例如 `aio_hips_write_snr_points_v2`），但这会让"40 字节、可错位"的结构继续留在公共头上，与 `ASTROCS_DESIGN §7.3` 的字面要求继续冲突 —— **不推荐**。
- **最小改动路径（按序）**：`aio_hips.h` 结构 + 版本常量 → `aio_hips_writer.cpp` 五个入口的前置校验 → `aio_abi_mirror.py` → C 探针 + `aio_abi_layout_lock` + `CMakeLists.txt` + `eng/ci/checks.json` → `hips_direct_smoke.py` 迁到镜像与正确 DLL 路径 → 两个门禁阈值修补。
- **影响面**：`lib/infrastructure/aio/include/aio_hips.h`（公共 ABI）、`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`、`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`（构 view 的两处）、`lib/infrastructure/aio/tests/hips_direct_smoke.py`、`tests/unit/p1_hips_writer_test.cpp`、`lib/infrastructure/aio/tests/p1hips/*.cpp`（构 view 的用例）、`eng/tools/check_abi_boundary.py`、`eng/tools/quality/check_module_map.py`、`eng/ci/checks.json`。**需要变更 claim：否**（新 ABI 版本号属实现登记）。
- **附带发现（不在本议题、登记即可）**：`docs/modules/MODULE_MAP.yaml:519/:522` 声明的 `lib/infrastructure/aio/CMakeLists.txt` 与 `lib/infrastructure/aio/module.yaml` **均不存在**（`ls` 报"没有那个文件或目录"），故 `check_module_map.py` 对 aio 输出 `missing_cmake_target / missing_module_yaml` —— 这也是**不能**把 ABI 门落在 module-map 通道、必须落在 ctest 的原因。

### 5.3 M2b-H-01 —— 唯一推荐：改精度（`%.17g`，两处共用同一函数），容差 `<1e-9` 保留

- **唯一推荐（0.85）**：新增单一格式化函数（建议 `static std::string fmt_sky_fraction(double)`，实现 `%.17g`），`aio_hips_writer.cpp:1023`（properties image）、`:1262`（properties SNR）、`:1538`（manifest.json）**三处全部**改用它；`docs/algorithms/HIPS_WRITER.md §9:296-300` 的 `<1e-9` **原文保留**，并补写「字面量由**唯一**格式化函数产出，且 round-trip 精确（`%.17g` ⇒ 解析回程误差恒 0）」。
- **为什么"改精度"而不是"改容差"**：
  1. 容差是**科学量**的验收界（MOC 覆盖面积的绝对精度）；为迁就一个**序列化**产物把它放宽 1000 倍（1e-9 → 1e-6），等于用工程债换科学指标，方向错误。
  2. 放宽到 1e-6 也**修不好**两处字面量不同（8 dp 与 6 dp 仍不同）、**修不好** K≥20 的数值湮灭（`0.000000`），且 **最大误差 5e-7 仍逼近 1e-6**（余量仅 2 倍）。
  3. 反过来，`%.17g` 让「字面量精确相等」与「<1e-9」**同时无条件成立**（误差恒 0），是一次到位的解。
- **为什么是 `%.17g` 而不是 `%.12f`**（`%.12f` 也满足 <1e-9）：C 的 `%g` 是**有效位**格式，`%.17g` 是 `DBL_DECIMAL_DIG`（IEEE-754 double 的 round-trip 位数）。固定小数格式（`%.12f`）在**小覆盖**处会把值打成 0（实测 `K=29,N=1`：`%.12f` → `0.000000000000`，`%.17g` → `2.8912057932946783e-19`），并且它虽满足绝对容差、却丢失了比例信息。选 `%.17g` 后，**"重跑后仍成立"的论证不依赖任何余量假设**。
- **「重跑后仍成立」的量化论证**（这是本议题的核心要求）：
  1. **可复现性（确定性）**：`moc_sky_fraction = moc_area_sr/4π = N_cells / (12·4^K)`，`N_cells` 是整数（`aio_hips_writer.cpp:660/:929` 逐叶 `insert` 去重计数）、`K` 是整数 ⇒ 值是关于整数元组的**纯函数**，无随机、无并行求和（`fixed_reduction_order`，`HIPS_WRITER.md:140-141`）⇒ 同一输入**必然**产出同一个 double（本轮两次独立运行 `out/mode1`、`out/mode2` 的 profile 与产物结构完全一致）。
  2. **序列化无损**：`%.17g` 满足 `strtod(snprintf("%.17g", v)) == v` 对**任意** double 成立（`DBL_DECIMAL_DIG = 17`，C11 5.2.4.2.2 / IEEE-754 十进制往返定理）。故 `|parse(lit) − v| = 0` **恒成立**，容差 `<1e-9` 的余量为 +∞；与量化步长无关，因此**不随 K、不随覆盖率、不随平台**变化。
  3. **双面一致**：properties 与 manifest 由同一函数产出 ⇒ 断言退化为**字符串精确相等**，与浮点比较无关；同一断言在任何重跑、任何机器、任何 locale（`%g` 的小数点受 `LC_NUMERIC` 影响——建议实现内固定 `"C"` locale 或改用手写转换，作为实现细节登记）下稳定成立。
  4. **可失败性（门有判别力）**：把断言点从 `3/12=0.25` 换到 `1/12` 后，6 dp 立刻红（误差 `3.333e-07` = 容差 333 倍，§2.5-E[D]），8 dp 红（`3.33e-09`），只有 `p ≥ 10`（或 `%.17g`）能绿 ⇒ 新门**能被现有实现证伪**，符合 `ENGINEERING_SPEC §8`「能红能绿」。
- **反方（最可能被反驳的点）**：
  1. 「`%.17g` 让 properties 变得不可读（`0.083333333333333329`）」——**成立但可接受**：真实 CDS properties 也直接写 `moc_sky_fraction = 1`；IVOA 规范（`REC §4.4.1`，抽文行 540-543）对值只要求 `keyword = value`、**不限字符数**、也不限数值格式。
  2. 「既有断言被改，等于放宽了自己的锁」——**不成立**：改动方向是**收紧**（6 dp→17 sig），且同时新增一个在 `1/12` 点上的红-绿可证门。
  3. 「`%.17g` 在不同 libc 上输出不同」——**不成立**：`%.17g` 是**精确十进制转换**（正确舍入），glibc 与 Python 的 `%.17g` 输出逐字节相同（我实测的 C 表与 Python 表一致）。
- **最小改动路径**：`aio_hips_writer.cpp` 加一个静态函数 + 3 个调用点；`HIPS_WRITER.md §9` 加一句限定语（**不需要变更 claim**：容差本身不变，只是明确其量测域包含序列化）；`p1hips_tests_units.cpp:292`、`p1hips_tests_properties.cpp:230`、`p1hips_tests_oracle.cpp:124-130` 的期望值改由同一格式化函数生成；`tests/io/make_hips_fixture.py:171` 从 `:.9f` 改为同一语义（建议 Python 侧 `"%.17g" % v`）。
- **影响面**：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`、`lib/infrastructure/aio/tests/p1hips/p1hips_tests_{units,properties,oracle}.cpp`、`tests/io/make_hips_fixture.py`、`docs/algorithms/HIPS_WRITER.md §9`。下游 `manifest.json` 消费者（若有按字符串比对的产品）需知悉 —— `grep` 显示只有 `lib/algorithms/coverage/src/sampler.cpp:353` 读 `moc_sky_fraction` 键名做**存在性**判断（不比较数值）。

### 5.4 M2a-H-3 —— 唯一推荐：归约改用未钳制的真实面积；I2 保留；补可观测计数

- **唯一推荐（0.80）**：
  1. **归约式改为面积守恒/面积加权**：在叶级主循环里额外保留**未钳制**的覆盖面积（新增 `area_n[i] = area` 缓存，或把 `sup_n` 存成**未钳制**值、只在写 FITS 时钳），层级累加改为
     ```
     flux_n += sig_n[i] * area_n[i];   // = flux（真通量，无钳制损失）
     area_n += area_n[i];              // 真覆盖面积（未钳制）
     ```
     父级归一 `sig_p = flux_n/area_n`、`sup_p = min(area_n/A_cell_k, 1)`（**只钳一次**）。
     —— 这是本报告认定的**正确归约式**：`signal` 必须是**面积加权**平均面亮度（守恒式），而不是 `support` 加权。
  2. **保留 I2**（`support = min(area/A_cell,1) ∈ [0,1]`）：这是 IVOA「覆盖比例」语义，不动。
  3. **补可观测**：新增 properties/manifest 键（建议 `astrocs_support_clamped_pixels` 与 `astrocs_coverage_gt1_pixels`），使"覆盖>1"从**不可观测**变为**可测量**（现 `grep -c support_clamped` = 0）。
  4. **冻结文本订正（需变更 claim）**：`DATA_SEMANTICS:346-347` 的「`F=signal×support×A_cell` 闭合（gate7 复检同式）」改为**两段**：(i) 无覆盖>1 时该式**精确闭合**；(ii) 存在覆盖>1 时，通量的**可达上限**为 `min(Σf_j, (Σf_j/Σa_j)·A_cell_k)`，如实登记为**编码限**（`support≤1` 的必然结果），**不得**再宣称"闭合"。
  5. **I7 的 oracle 独立化**：`p1hips_tests_properties.cpp:95` 的 oracle 必须换成独立式（面积加权、未钳制），并且**不得**与实现共用同一段表达式。
- **量化论证（供决策，全部来自真实生产二进制，§2.5-B/C）**：
  - 均匀覆盖 `c`（同一父 cell 内所有叶像素同 `c`）：生产父级通量损失 = `1 − 1/c`，实测 `c=1.25 → 20.00%`、`1.5 → 33.33%`、`2 → 50.00%`、`4 → 75.00%`、`8 → 87.50%`；**该损失不可由任何归约式消除**（受 `support≤1` 编码限），本报告已如实标注为"不可避"。
  - 异质覆盖（同父 cell 内 `c` 不同）：生产用钳后 `sup` 当权重 ⇒ 父级面亮度偏离真面积加权值，实测 `(10@c=4, 0.1@c=1)`：真 `8.02` vs 生产 `5.05`（低 37.0%），父级通量相对可达上限的**可避免损失 37.032%**；`(1000, 0.001)` 时可达 **37.5%**；极限情形可逼近 100%（把 `c>1` 子像素占全部通量即可）。
  - **不回归风险**：`20000` 组随机 `c∈(0,1]` 样本上，生产式与推荐式的 `sig_p` 相对差最大 `4.994e-16`（纯浮点重结合）⇒ 修复**不改变**当前所有"无覆盖>1"产品的科学值，只影响 `f64` bitwise 断言的**最后一位**（故 I7 的 bitwise oracle 必须同步改，否则会假红）。
  - `a_j > A_cell` 的**可达性**：由 `astro_sphere_sink.cpp:124-125` 直写面 `dense_area[local] = acc.sumArea`（`sumArea` 是**多帧累加**，IVOA §4.2.1.1 明确讨论"overlapping multiple input original images"）⇒ 多帧重叠区必然 `Σa_j > A_cell`。**不是理论构造**。（另一条 Phase1 面 `astro_sphere_sink.cpp:286-290` 在 sink 就钳到 `A_cell`，故该面**不触发**本缺陷 —— 修复须区分两条面。）
- **反方（最可能被反驳的点）**：
  1. 「`support≤1` 是设计，不是缺陷，所以本条整体不成立」——**部分成立**：均匀覆盖的那部分确实不是缺陷（我已量化并把"可避免=0"如实标出）；但**异质覆盖下用钳后值当权重**是纯粹的实现错误，且它使父级面亮度既非面积加权也非通量守恒，**没有任何口径能自圆其说**。所以裁"整条不成立"会漏掉真缺陷，裁"整条成立"会冤枉编码限——本报告给的是**分离后的唯一修法**。
  2. 「改归约式会让 bitwise 断言变红，成本高」——**成立**，故建议与 oracle 独立化**同批**改，并明确 `I7` 的 f64 域断言应改为"与独立 oracle bitwise"（oracle 用新式），f32 域保持 `rtol=1e-6`。
  3. 「也许该把 support 放开到 >1」——**不推荐**：`support` 是 IVOA「覆盖比例」，`DATA_SEMANTICS:324` 与 `IO_002` 读端都按 `[0,1]` 合同；放开会波及 `DATA_SEMANTICS:1149/:1162/:1178` 的 Phase2 消费语义（那里 `support` 语义是 `[0,1]`）。**编码限应当声明而非拆除**。
- **最小改动路径**：`aio_hips_writer.cpp`（叶级主循环 + `finalize_hierarchy` + properties/manifest 两个写点）→ `p1hips_tests_properties.cpp` 的 oracle 独立化 → `HIPS_WRITER.md §3/§4/§9`（(4b)(4c)/I7/I11/冻结容差）与 `DATA_SEMANTICS §12.2/§20.3`（**变更 claim**）。
- **影响面**：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`、`lib/infrastructure/aio/tests/p1hips/{properties,oracle,units}.cpp`、`docs/algorithms/HIPS_WRITER.md`、`docs/contracts/DATA_SEMANTICS.md`；`astro_sphere_sink.cpp` **不需要**改（它只是数据源）。**下游收益**：Phase2 `signal/support` 逆变换（`DATA_SEMANTICS:1034-1040`）在多帧重叠区的通量恢复率提升（实测该算例 +37%）。

### 5.5 M9-F-3 —— 唯一推荐：闭域 + fail-closed + 结构化诊断

- **唯一推荐（0.75）**：`aio_hips_set_drizzle_provenance` 的参数校验改为
  ```c
  if (!ps) return 1;
  if (!(pixfrac > 0.0 && pixfrac <= 1.0)) { set_error("pixfrac 必须在 (0,1]"); return 2; }   // 现状已拒 NaN/Inf
  if (!std::isfinite(scale_arcsec))       { set_error("scale_arcsec 非有限 (NaN/Inf)"); return 2; }
  if (!(scale_arcsec > 0.0))              { set_error("scale_arcsec 必须 > 0"); return 2; }
  if (scale_arcsec > ACS_HIPS_MAX_FRAME_SCALE_ARCSEC)
                                          { set_error("scale_arcsec 超出物理域"); return 2; }
  ```
  **推荐上下界**（唯一取值）：
  | 界 | 值 | 依据 |
  |---|---|---|
  | 下界 | `> 0`（声明值 `3.93e-4″`，非绑定） | 像素角尺度为正值；`3.93e-4″` = 最细合法 HiPS 叶像素（`nside = 2^29`，上界由 M2b-A-01 已冻结），输入帧尺度按 SCI-DRZ-001 的 `1–2×` 过采样必 ≥ 它 |
  | 上界 | **`824.5167388361774″` ≈ 824.52″**（=`2 × 412.258369″`） | `412.258369″ = 3600·180/π·√(π/3)/512` 是 **`nside ≥ 512` 下最粗的合法 HiPS 叶像素**（`DATA_SEMANTICS:349-350` 的 `hips_pixel_scale` 公式 + `aio_hips.h:101`「叶级 NSIDE ≥ 512」+ M2b-A-01 已强制）；SCI-DRZ-001 冻结 `1–2×` 过采样 ⇒ 输入帧尺度 ≤ 2× 该值 |
- **拒绝语义（三条硬要求）**：
  1. **一律 `return 2`**（与 `pixfrac` 同码，保持该入口的错误码语义简单）；
  2. **每次拒绝必须 `set_error(...)`**（`ENGINEERING_SPEC:137`），且错误串要含具体原因；
  3. **删除"接受 0 但静默不写键"的路径**：既然 `0` 现在被拒，`aio_hips_writer.cpp:1012` 的 `if (ps->drizzle_scale_arcsec > 0.0)` 分支应改为**无条件写**（`provenance` 仍是全或无：`drizzle_prov_set` 为真 ⇒ 两个键**必须**都在），与 `aio_hips.h:184-185` 自述的「全或无…禁静默缺键」纪律一致。
  4. （配套）序列化 `%.4f` 与本域下界不自洽（1e-4 量化 vs 3.93e-4 下界 ⇒ 细端相对误差可达 25%），建议改 `%.8g`；同为不动容差、只动字面量。
- **为什么不用 k_corr 的 `[300,600]″` 当作域**（这点很关键，容易被误选）：那是**消费侧**（`PHASE2_SAMPLER.md:191-193`）的**查表适用域**，而生产实测尺度 ≈ `0.9586″/px`（`OWNER_DECISIONS.md D-10`）**本就在域外**。writer 的职责是**如实记录**帧的真实尺度，因此拒绝域必须是**物理/表示域**，不能是查表域；域外值应由 sampler 按 `ALG-P2-SURF-UPM.md:83`「禁止外推/内插」显式拒绝，而不是让 writer 把真实值藏起来。
- **反方（最可能被反驳的点）**：
  1. 「`824.52″` 这个上界是推导出来的，不是外部标准 ⇒ 有误杀风险」——**成立**，故置信度只给 0.75。缓解：(i) 该值由**仓内已冻结常量**（叶 `nside ≥ 512`、`hips_pixel_scale` 公式、SCI-DRZ-001 的 2× 上限）唯一确定，非拍脑袋；(ii) 即使误杀，代价是一次 `rc=2` 的显式失败（可见、可诊断），远比当前"NaN 静默通过、键静默缺失"安全；(iii) 若不接受该值，本报告建议的**次优**是 `1° = 3600″`（保守三倍），但**必须**同时保留 `isfinite` 与 `>0` 两条硬校验。
  2. 「0 可能被调用方当作'尺度未知'的哨兵」——**不成立**：若是"未知"，正确做法是**不调用**该 setter（provenance 全或无），而不是传 0 让 writer 猜；现有 `>0 才写` 分支正是这个歧义的来源。
  3. 「拒绝 0 会打破既有调用方」——**风险低**：全仓 `set_drizzle_provenance` 调用点只有 `astro_sphere_sink.cpp:85`、`tests/unit/p1_hips_writer_test.cpp:70`、`p1hips_tests_units.cpp:183`、`p1hips_tests_negative.cpp:269-274`，传入的 scale 分别是 `config` 值 / `1.0` / `0.35` / `{1.0,1.0,-1.0,0.1}` —— **没有一个传 0**。
- **最小改动路径**：`aio_hips_writer.cpp:1318-1319` + 常量定义（建议置于 `aio_hips.h` 便于门读取）+ `aio_hips_writer.cpp:1012` 的无条件写 + `p1hips_tests_negative.cpp` 增 4 条负例与 1 条正例 + `HIPS_WRITER.md:293-296` 负面矩阵补 `drizzle_scale_arcsec` 行。
- **影响面**：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`、`lib/infrastructure/aio/include/aio_hips.h`、`lib/infrastructure/aio/tests/p1hips/p1hips_tests_negative.cpp`、`docs/algorithms/HIPS_WRITER.md`。**需要变更 claim：否**。

### 5.6 需负责人确认的那一句话

> **请确认：把 `AioHipsSnrPoint`（及另外 3 个 AIO 跨边界结构）的首部加上 `struct_size`/`abi_version`，即**改变其二进制布局、使现行 40 字节版本不再向后兼容**，并以 `ipv_*` 同款的「C 探针 + 唯一 Python 镜像 + 逐字段布局锁 + 变异自检」门把它锁死。**
> 支撑事实（本轮实测）：仓内唯一的 Python 消费者 `lib/infrastructure/aio/tests/hips_direct_smoke.py:43` 硬编码的 `lib\astro_image_io\astro_image_io.dll` 在 ARCH-001 迁移后**已不存在**（该目录已无）⇒ 该消费者当前**完全无法加载**，ABI 破坏面是**理论面**；C++ 侧唯一调用点与 writer 同仓同版本、必须同编译。若不接受，则退路为「新增 v2 入口、保留 v1」，但 40 字节可错位结构将继续留在公共头上，与 `ASTROCS_DESIGN §7.3` 冲突。

---

## 6. 证据清单

> 全部命令在 `/workspace/Astro CS Database` 下执行；`run/PROJECT-GOVERNANCE-01/R-7/` 为本研究线**唯一**写入目录（外加交付报告 `reports/PROJECT-GOVERNANCE-01/research/R-7_AIO_HIPS_ABI与精度.md`）。**未修改仓库任何被 git 跟踪的文件**；`git status` 中出现的改动全部来自 AIO-001 既有的未提交工作树，非本轮产生。

### 6.1 命令与退出码

| # | 命令 | rc | 产物 / 输出 |
|---|---|---|---|
| 1 | `git log -1 --format='%H %ad %s'` | 0 | `e9b31285…`（见报告头） |
| 2 | `ctest -R "p1hips" --output-on-failure`（`build/`） | 0 | `100% tests passed, 0 tests failed out of 6`（基线绿） |
| 3 | `curl -sSL -o REC-HIPS-1.0-20170519.pdf http://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf` | 0 | `REC-HIPS-1.0-20170519.pdf`（2 924 499 B，32 页） |
| 4 | `PYTHONPATH=/tmp/pdflib python3 extract_pdf.py …` | 0 | `REC-HIPS-1.0-20170519.txt`（`pages: 32 chars: 56887`） |
| 5 | `grep -n -i -e 'tileWidth' -e 'ORDERING' -e 'FITS keyword' REC-HIPS-1.0-20170519.txt` | 0 | 命中行 352/355/456/542/734 —— **`NSIDE` 0 命中** |
| 6 | `curl … https://aladin.cds.unistra.fr/hips/HipsgenReferenceManual.html` | 0 | `hipsgen.txt`；`sed -n '922,929p'` → `nside = tileWidth x 2^order` |
| 7 | `curl … alasky.cds.unistra.fr/{DSS/DSS2Merged,2MASS/H,AKARI-FIS/N160,ACT/DR4DR6/f090}/Norder{0,2,3,4}/Dir0/Npix0.fits` | 0 | 12 个真实 tile + 3 份 `properties`；`tiles with NSIDE card: 0` |
| 8 | `python3 fetch_tiles.py`（astropy 7.0.1 读头） | 0 | 见 §2.3 表 |
| 9 | `python3 parse_list.py` / `scan_nside.py` | 0 | `rows: 1584` / `fits-capable HiPS: 1208` / `tiles with NSIDE card: 0` |
| 10 | `g++ -O2 -std=c++17 -I lib/infrastructure/aio/include -o probe_abi probe_abi.cpp build/libastrocs_hips.a build/libastrocs_aio.a -lgomp -lpthread build/libastrocs_cfitsio.a -lz build/libastrocs_common.a` | 0 | `probe_abi`（1 869 088 B） |
| 11 | `./probe_abi out/abi` | 0 | `sizeof(AioHipsSnrPoint)=40` / `sizeof(PyMirror)=32` / `rc=0` ⇒ TSV 第 2、3 条错位（§2.5-A） |
| 12 | `cat out/abi/snr/Norder0/Dir0/Npix4.tsv` | 0 | 逐字见 §2.5-A |
| 13 | `g++ … -o probe_m2ah3 probe_m2ah3.cpp …` | 0 | `probe_m2ah3`（1 869 088 B） |
| 14 | `./probe_m2ah3 out/mode1 1 m1` / `./probe_m2ah3 out/mode2 2 m2` | 0 | `OK label=m1 mode=1 nside=1024 A_cell=9.9868540877971423e-07 tile_order=1`（两行） |
| 15 | `python3 analyze_m2ah3.py \| tee logs/m2ah3_analysis.log` | 0 | 均匀损失表 + 异质 74.813% / 可避免 37.032% |
| 16 | `python3 numpy_m2ah3.py \| tee logs/m2ah3_numpy.log` | 0 | 对拍相对差 `0.000e+00` / `5.680e-08`；`[D]` 最大相对差 `4.994e-16` |
| 17 | `g++ -O2 -std=c++17 -o moc_precision moc_precision.cpp && ./moc_precision` | 0 | `logs/moc_precision_raw.log`（§2.5-D 表） |
| 18 | `python3 moc_precision.py \| tee logs/moc_precision.log` | 0 | §2.5-E 全部结论 |
| 19 | `python3 -c "…3600*180/pi*sqrt(pi/3)…" \| tee logs/scale_domain.log` | 0 | `412.258369″ / 0.000393″ / 824.5167388361774″` |
| 20 | `python3 eng/tools/check_abi_boundary.py` | **0** | `CPU-001_PASS: ABI 结构 11 个均带 size/version; 11 头无 STL/异常跨边界` —— **`aio_hips.h` 不在扫描面** |
| 21 | `python3 eng/tools/quality/check_module_map.py` | **0** | aio 行：`NOT_IMPLEMENTED … missing_cmake_target missing_implementation missing_module_yaml product_unit_skeleton` —— **无 `header_missing_abi_version`**（被 `aio_pipeline.h:84-85` 的 token 绕过）；总体 `… 其余 113 条 FAIL` 仍 rc=0 |
| 22 | `grep -rn "struct_size\|abi_version" lib/infrastructure/aio/include/` | 0 | 仅 `aio_pipeline.h:84/:85/:113`；`aio_hips.h` **0 命中** |
| 23 | `grep -n -e struct_size -e abi_version lib/infrastructure/aio/include/aio_hips.h` | **1** | **0 命中**（缺版本头） |
| 24 | `grep -c "support_clamped" lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` | **1** | `0`（钳制不可观测） |
| 25 | `grep -n 'NSIDE=2\^k' docs/algorithms/HIPS_WRITER.md` | 0 | `146:` |
| 26 | `grep -n 'NSIDE. \| 若存在必须等于' docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md` | 0 | `104:` |
| 27 | `grep -n 'nside=2\^(k+9)' docs/contracts/DATA_SEMANTICS.md` | 0 | `327:` |
| 28 | `grep -rn "drizzle_scale_arcsec\|DRIZZLE_SCALE_ARCSEC" docs/ contracts/ eng/tools/ cli/` | 0 | **仅 `docs/algorithms/HIPS_WRITER.md:184` 一处** |
| 29 | `ls lib/infrastructure/aio/module.yaml lib/infrastructure/aio/CMakeLists.txt` | **2** | 两者均"没有那个文件或目录" |
| 30 | `grep -rn "set_drizzle_provenance" tests/ lib/ …` | 0 | 调用点 5 处，**无一处传 `scale=0`** |
| 31 | `sed -n '1312,1326p' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` | 0 | 两处 `return 2;` 前**无 `set_error`** |

### 6.2 产物路径

```
reports/PROJECT-GOVERNANCE-01/research/R-7_AIO_HIPS_ABI与精度.md      ← 本交付物
run/PROJECT-GOVERNANCE-01/R-7/
├── REC-HIPS-1.0-20170519.pdf / .txt        IVOA HiPS 1.0 REC 原文与抽文（32 页 / 56 887 字符）
├── extract_pdf.py                          抽文脚本（pypdf 6.19.0）
├── hipsgen.txt                             CDS Hipsgen 参考手册（nside = tileWidth × 2^order）
├── hipslist.txt / parse_list.py            1584 行 CDS HiPS 列表与解析
├── fetch_tiles.py / scan_nside.py          12 个真实 tile 下载 + 头卡扫描
├── hips_samples/                           12 个真实 FITS tile + 3 份真实 properties
├── probe_abi.cpp / probe_abi               V11-N-01 ABI 错位探针（链真实静态库）
├── probe_m2ah3.cpp / probe_m2ah3           M2a-H-3 层级通量探针（链真实静态库）
├── analyze_m2ah3.py                        产物回读复算
├── numpy_m2ah3.py                          NumPy 独立复算 + 对拍 + 推荐式对比
├── moc_precision.cpp / moc_precision        std::to_string vs %.8f/%.12f/%.17g（真 libstdc++）
├── moc_precision.py                        容差↔精度匹配律 与 恒绿性证明
├── out/mode1|mode2|abi/                    探针产物（含 properties / manifest.json / TSV）
└── logs/
    ├── m2ah3_analysis.log                  真实二进制层级通量损失表
    ├── m2ah3_numpy.log                     NumPy 复算与对拍
    ├── abi_probe.log                       sizeof/offsetof + 错位结果
    ├── moc_precision_raw.log               C++ 字面量矩阵
    ├── moc_precision.log                   容差/精度/恒绿/湮灭结论
    ├── scale_domain.log                    drizzle_scale_arcsec 物理域推导
    └── （PDF 抽文、tile 下载、列表解析的原始输出见报告 §2 各表）
```

### 6.3 交付自检

| 硬约束 | 状态 |
|---|---|
| 零 git 写 | ✅ 未执行任何 `git add/commit/push/checkout` |
| 不修改仓库任何文件 | ✅ 只写 `reports/PROJECT-GOVERNANCE-01/research/` 与 `run/PROJECT-GOVERNANCE-01/R-7/`；`/tmp/pdflib` 为 pypdf 的临时安装目录（仓外） |
| 不采信账本 `fix_state/verified_state` | ✅ 全部 5 条均在本轮当树复跑/实测；AIO-001 的行号锚已实测失效并订正 |
| 给唯一推荐、不以「需负责人裁决」当结论 | ✅ §0 一页表 5 条全部给出唯一推荐 + 置信度 + 反方 + 最小改动路径 + 影响面 |
| 尽量取规范原文并引用 | ✅ IVOA HiPS 1.0 REC 原文（PDF 抽取，逐字引用 §4.2.1 与表 5）+ CDS Hipsgen 手册 + 12 个真实发布 HiPS 产物 |
| 能跑实验的必须跑 | ✅ 4 个独立实验（真实库 C 探针 ×2、NumPy 复算、真 libstdc++ 字面量探针），全部带命令与逐字输出 |
