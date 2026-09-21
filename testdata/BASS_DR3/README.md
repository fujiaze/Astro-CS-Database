# BASS DR3 single_image 归档索引

本目录是针对 China-VO 镜像站 BASS DR3 **单帧图像**归档构建的本地索引，并附坐标索引、下载工具与说明。

## 1. 数据集概览

- **数据源**：<https://casdc.china-vo.org/archive/BASS/DR3/single_image/>
- **巡天**：BASS（Beijing–Arizona Sky Survey，2.3 m Bok 望远镜 + 90Prime 相机，g/r 两波段，BASS 的 z 波段由 MzLS 承担）
- **DR3 论文**：*The Third Data Release of the Beijing-Arizona Sky Survey*, [arXiv:1908.07099](https://arxiv.org/abs/1908.07099)
- **归档组织**：`single_image/<YYYYMMDD>/` 按观测日期分目录；每帧文件名为 `<pointing>_<ccd>[.wht|_od].fits.fz`，全部为 fpack 压缩 FITS
- **访问方式**：本目录所有工具**强制直连、不使用代理**（`trust_env=False` + 清理代理环境变量 + `--noproxy`）

## 2. 三类文件（science / weight / od）

每次曝光（一个指向 × 一个 CCD）对应 **3 个文件**，共享同一曝光头（90Prime、g/r 波段、曝光时长、WCS），按后缀区分：

| kind | 文件名示例 | 内容 | 实测证据 |
| --- | --- | --- | --- |
| `science` | `p7030g0031_1.fits.fz` | 校准后的单帧科学图像（流量单位 e/s，含 WCS） | `IMAGETYP='object'`、float32、`FILTER='g'`、`EXPTIME=90`、完整 `RA---TAN/DEC--TAN` |
| `weight` | `p7030g0031_1.wht.fits.fz` | 权重图 = 逐像素**逆方差**（1/σ²） | 头与 science 完全一致；DR2/DR3 论文：*"The weight map provides the inverse variance of flux for each pixel."* |
| `od` | `p7030g0031_1_od.fits.fz` | **flag/掩码图**（8-bit 位掩码） | `ZBITPIX=8`、`ARTIFACT='Y'`、`HASSAT=T`、`NUM_COSM=2321`；论文：*"The flag image tags the problematic pixels"* |

### od 与 weight 是如何生成的

来自 BASS DR3 数据归约管线（论文 III.1），对原始 CCD 依次做 9 步处理后，**最后一步统一产出三张图**：

1. 过扫区缺失行修复 → 2. 串扰（crosstalk）校正 → 3. 过扫与偏压扣除（中值偏压，20 张偏压帧）→ 4. 平场（穹顶平场 + 超天光平场照度校正）→ 5. 放大器增益平衡 → 6. 图案噪声去除（仅 z 带，低通 Butterworth）→ 7. 瞳孔鬼影去除（仅 z 带，DR3 新增）→ 8. CCD 伪影识别（坏像素、饱和、宇宙线、卫星轨迹）→ 9. 对坏像素/伪影做双线性插值。

第 9 步末尾产出三图：**detrended image**（即 science）、**weight map**、**flag map**（即 od）。weight 的物理定义论文只给了"逆方差"，没有给出具体公式；其噪声模型成分（读出噪声 `RDNOISE`、增益 `GAINONE`、天光 `SKY`/`SKYRMS`）都在 FITS 头里。flag 图按位标记：**1=坏像素、2=饱和、4=宇宙线、8=卫星轨迹**（论文原文）。镜像站未对 `od` 后缀作文字说明，此命名含义为基于论文 + 文件头的推断。

## 3. 索引产物

```
BASS DR3/
├── index.json              # 索引总表 (元数据 + 汇总 + 按日期聚合, ~20 KB)
├── index.csv               # 全量扁平索引 552,300 行 (32.7 MB, 紧凑字段, 无 URL)
├── index.csv.gz            # gzip 压缩版 (2.6 MB)
├── coords.csv              # 每帧坐标索引 184,100 行 (46.7 MB, join ccdinfo)
├── coords.csv.gz           # gzip 压缩版 (12.4 MB)
├── SHA256SUMS.txt          # 全部产物 SHA-256 指纹
├── dates/<YYYYMMDD>.json   # 按日期分片 (353 个, 无 URL)
├── data/bassmzls-dr3-ccdinfo.fits   # 坐标来源表 (245 MB, 下载一次)
└── tools/
    ├── crawl_bass_dr3.py        # 爬取归档列表构建索引
    ├── build_coords_index.py    # 下载 ccdinfo 并构建坐标索引
    ├── download_subset.py       # 子集下载 (可只下 science) + funpack
    ├── verify_bass_dr3.py       # 一致性 + 抽样下载校验
    ├── test_aio_fz.py           # aio fpack(.fz) 读取验证
    ├── analyze_footprint.py     # 覆盖天区/体积/HiPS 规模分析
    ├── constellation_coverage.py# 星座归属分析 (用 tools/ 内 Roman 1987 边界表)
    └── roman1987_constellation_boundaries.tsv  # VizieR VI/42 星座边界 (随工具入库)
```

### 仓库跟踪策略（已并入 main 主线）

- **入仓库**：`tools/`（7 个脚本 + 星座边界表）、`index.json`、`index.csv.gz`、`coords.csv.gz`、`constellation_coverage.csv`、`dates/*.json`、`README.md`、`SHA256SUMS.txt`。
- **不入仓库（本地保留/可再生成）**：`index.csv` / `coords.csv`（未压缩版，仓库内用 `.gz`）、`data/`（ccdinfo 245 MB）、`logs/`。
- 克隆后如需运行脚本，先解压两个 `.gz`：

```powershell
py -3.12 -c "import gzip; [open(f,'wb').write(gzip.open(f+'.gz','rb').read()) for f in ['index.csv','coords.csv']]"
```

### index.csv 字段（紧凑命名）

| 字段 | 含义 | 备注 |
| --- | --- | --- |
| `d` | 日期目录 `YYYYMMDD` | |
| `f` | 文件名 | `<pointing>_<ccd>[.wht\|_od].fits.fz` |
| `s` | 服务端列出的字节数 | |
| `k` | 类型 | `science` / `weight` / `od` |
| `p` | 指向编号 | 如 `p7030g0031` |
| `c` | CCD 序号 | 1–4 |

URL 不再存储，按 `https://casdc.china-vo.org/archive/BASS/DR3/single_image/<d>/<f>` 拼接即可。

### coords.csv 字段（每帧坐标，不下载图像即可获得）

坐标来自归档的 `files/bassmzls-dr3-ccdinfo.fits`（429,111 行 × 79 列，单次下载 245 MB），按**文件名**与 science 图像 join（184,100/184,100 全部匹配）。字段：

| 字段 | 含义 |
| --- | --- |
| `f`,`d` | 文件名与日期目录 |
| `ra`,`dec` | 帧中心坐标（度，J2000） |
| `ra_obs`,`dec_obs` | 指向中心坐标 |
| `ra_lb..dec_rb` | 四角坐标（lb=左下、lt=左上、rt=右上、rb=右下） |
| `pxl`,`see`,`am`,`exp` | 像素尺度(″/px)、seeing、气质量、曝光时间 |
| `filt`,`mjd`,`date`,`time` | 波段、MJD、观测日期(UTC)、时刻 |
| `zpt`,`zpt_rms` | 测光零点及其 RMS |
| `astr_ra_rms`,`astr_dec_rms` | 天体测量残差 RMS |
| `imq`,`survey`,`pass` | 图像质量标记、巡天(bass)、处理版本 |

注意：
- ccdinfo 的 `date` 是 **UTC 日期**，与归档目录日期（本地日期）在跨午夜时可能差一天（如目录 20150111 对应观测 2015-11-12 的曝光）；**join 键是文件名而不是日期**。
- 约 1.5% 的行（`imq=5`）缺测量值：`see/zpt/pxl` 为 0 或异常大数，使用前需过滤（主要分布在 20150315、20160216 等日期）。

## 4. 索引统计（2026-08-12 构建）

| 指标 | 数值 |
| --- | --- |
| 日期目录数 | 353（20150107 – 20190306） |
| 文件总数 | 552,300（science/weight/od 各 184,100） |
| 总大小 | 3,803,943,182,400 B（≈ 3.80 TB） |
| 坐标索引 | 184,100 行全部 join 成功 |
| 波段分布 | g 103,148 / r 80,416 / sdssr 536 |
| 抓取失败 | 0 |

## 5. 覆盖天区

基于 coords.csv 每帧四角坐标的栅格化分析（0.05° 网格，剔除 49,812 帧角点缺失/退化数据）：

| 指标 | 数值 |
| --- | --- |
| 巡天足印（归档并集） | **≈ 5,789 deg²**（考虑栅格边界高估，与论文 δ>30° 的 5,400 deg² 一致） |
| 主足印 DEC | δ > 20°（5,675 deg²），其余为赤道/南天测试区（COSMOS、Stripe 82 等，~114 deg²） |
| 主足印 RA | ~23°–300°，集中 6h–19h（北银冠） |
| 覆盖深度 | 平均 ~8 次 CCD 曝光/0.05° 格（含全部重观测帧；正式发布足印多为 3 次抖动覆盖，g 带约 1/3 区域 >3 次） |
| 指向/曝光 | 45,966 个指向 × 4 CCD = 183,864 次 CCD 曝光（另有 236 条跨日期重复条目） |
| 波段 | g 103,148 / r 80,416 / sdssr 536（science 计数） |

注意：**归档并集 > 正式发布足印**。single_image 目录包含所有已观测帧（重观测、早期浅数据、外协观测），而最终目录/叠加足印是筛选后的 5,400 deg²。

### 星座覆盖（Roman 1987 IAU 边界）

用 VizieR VI/42（Roman 1987）星座边界表，将足印格点由 J2000 岁差转到 B1875 后逐格归属（工具 `tools/constellation_coverage.py`，结果存 `constellation_coverage.csv`；已用 12 颗亮星验证）。主要覆盖：

| 星座 | 面积 (deg²) | science 帧数 | 星座 | 面积 (deg²) | science 帧数 |
| --- | --- | --- | --- | --- | --- |
| 大熊座 UMa | 1,351 | 38,925 | 天琴座 Lyr | 179 | 4,067 |
| 天猫座 Lyn | 753 | 18,441 | 北冕座 CrB | 152 | 4,405 |
| 武仙座 Her | 696 | 16,998 | 双子座 Gem | 119 | 3,603 |
| 猎犬座 CVn | 602 | 15,181 | 巨蟹座 Cnc | 72 | 1,903 |
| 牧夫座 Boo | 579 | 14,874 | 天鹅座 Cyg | 50 | 747 |
| 天龙座 Dra | 551 | 27,646 | 后发座 Com | 49 | 1,020 |
| 御夫座 Aur | 238 | 6,401 | 鲸鱼座 Cet | 42 | 1,165 |
| 小狮座 LMi | 231 | 7,123 | 双鱼座 Psc | 39 | 1,259 |

其余小面积：狮子座 36、鹿豹座 14、天秤座 9、六分仪座 7、麒麟座 6、波江座 3、宝瓶/白羊/金牛/三角座各 2、猎户座 1（deg²）。鹿豹座面积小但帧数多（10,830），因其细长条带被密集覆盖。

## 6. 体积与 HiPS 规模估算

### 归档体积（2026-08-12 服务端列示）

| 类型 | 文件数 | 归档(.fits.fz) | funpack 后(标准 FITS) |
| --- | --- | --- | --- |
| science | 184,100 | 1.88 TB | 12.16 TB（float32 4096×4032） |
| weight | 184,100 | 1.87 TB | 12.16 TB（float32） |
| od | 184,100 | 0.05 TB | 3.04 TB（uint8） |
| **合计** | 552,300 | **3.80 TB** | **27.36 TB** |

全量注册主线不现实：仅 science funpack 后就有 12.16 TB。建议按试点规模注册（见下）。

### HiPS 数据库规模（按 5,789 deg² 足印，512×512 瓦片）

HiPS 是金字塔：最深级像素尺度决定总量。90Prime 原生 0.455″/px 对应最深 **L10**：

| 最深层级 | 像素尺度 | 覆盖像素 | 覆盖瓦片 | float32 | float64 | RICE 压缩估 |
| --- | --- | --- | --- | --- | --- | --- |
| L6 | 6.4″ | 1.8 G | ~7k | 7 GB | 14 GB | ~3 GB |
| L7 | 3.2″ | 7.2 G | ~28k | 29 GB | 58 GB | ~10 GB |
| L8 | 1.6″ | 28.9 G | ~110k | 116 GB | 231 GB | ~40 GB |
| L9 | 0.8″ | 115.7 G | ~441k | 463 GB | 926 GB | ~160 GB |
| L10（原生） | 0.4″ | 462.9 G | ~1,766k | 1,851 GB | 3,703 GB | ~600 GB |

金字塔累计（L0–L8，1.6″/px 最深）：~154 GB float32 / ~309 GB float64。**结论：HiPS 库大小由最深层级主导；若目标 1.6″/px（L8），单数据库约 116–230 GB；要做到原生 0.455″/px（L10）则需 ~1.9–3.7 TB。**

### 流式处理内存估算（单帧 4096×4032）

| 阶段 | 内存估算 | 说明 |
| --- | --- | --- |
| 单帧读取（funpack 后 FITS） | ~132 MB（FP32）/ ~198 MB（FP64） | aio 先读原始 66 MB，再转 float32(66 MB)/float64(132 MB) |
| 单帧完整管线处理 | ~300–500 MB（FP32）/ 0.5–1 GB（FP64） | 读数 + 校准 + drizzle 累积缓冲 |
| 16 线程 OpenMP 批处理 | ≈ N 帧 × 单帧量 | N 帧并发时线性放大 |
| HiPS 瓦片累积 | 活跃瓦片数 × 1 MB(float32) | 必须按天区子块/瓦片流式 flush，不可整层缓冲（L8 全足迹 110k 瓦片 = 110 GB） |

以上为估算，建议先跑试点（如单个指向或一个 0.68° block）实测后再定注册规模与 nside。

### 试点规模建议

| 单位 | 帧数(science) | .fz 体积 | funpack 后 |
| --- | --- | --- | --- |
| 1 个指向 | 4 | ~40 MB | ~265 MB |
| 1 个 field（3 次抖动） | ~12 | ~120 MB | ~0.8 GB |
| 1 个 0.68° block | ~12–24 | ~0.2–0.3 GB | ~1.5 GB |
| 1 夜（示例 20190212） | 484 | ~4.9 GB | ~32 GB |

## 7. 归档重复项（重要发现）

`p7338g0029…p7338g0087` 共 **59 个指向 × 4 CCD** 的 236 个 science（及对应 weight/od）同时出现在 **20150111** 与 **20151111** 两个日期目录，但两份文件**内容不同**（大小与哈希均不同）。经头文件与 ccdinfo 交叉验证：这些曝光真实观测日期为 **2015-11-12（MJD 57338.07）**，即 **20150111 目录是错误归档副本**，20151111 才是正确位置。索引忠实保留两处条目；选择测试数据时应取 20151111。

## 8. 主线项目使用建议（science 是否够用）

**可以只下载 science**。BASS science 帧是已校准（去趋势、含 WCS）的单帧，适合作为主线 AstroCS 的输入测试数据，覆盖：

- 单帧读取 / 头部解析 / 格式兼容测试
- plate solving（头文件 WCS 可作为参考真值）、测光定标、SNR 估算
- Drizzle / HiPS 重投影几何测试

**仅 science 不足的场景**：Phase2 UPM 的 **SNR-aware 加权叠加**需要 weight 图（逆方差权重是叠加的组成部分）；od/flag 图可作为坏像素/宇宙线/卫星轨迹的掩膜参考（与 Phase2 迭代排异逻辑对应）。做这两类测试时建议同时下载 weight（`--kinds science,weight`）。

**格式前提（已解决）**：归档文件是 fpack `.fits.fz` 压缩格式。主线 `astro_image_io` 已升级支持 `.fz`（CFITSIO 透明解压，见模块 memory 2026-08-12 条目），可直接读取 science/weight/od 三类文件，不再需要先 funpack；`download_subset.py --funpack` 仍可选用作转换。

## 9. 使用方法

按日期查询（注意紧凑字段）：

```powershell
Import-Csv "BASS DR3\index.csv" | Where-Object d -eq "20190212" | Measure-Object
```

按指向查三件套：

```powershell
Import-Csv "BASS DR3\index.csv" | Where-Object p -eq "p7030g0031" | Format-Table d,f,s,k,c
```

只下载某个指向的 science 并自动 funpack 转换：

```powershell
py -3.12 "BASS DR3\tools\download_subset.py" --pointing p7030g0031 --kinds science --funpack --out "BASS DR3\downloads"
```

按日期范围下载 science + weight：

```powershell
py -3.12 "BASS DR3\tools\download_subset.py" --dates 20151111 --kinds science,weight --out "BASS DR3\downloads"
```

按天区（中心坐标框）挑选 science：

```powershell
py -3.12 "BASS DR3\tools\download_subset.py" --box "36.5,47.8,37.2,48.4" --limit 100 --out "BASS DR3\downloads"
```

## 10. 重新构建与校验

```powershell
py -3.12 "BASS DR3\tools\crawl_bass_dr3.py" --workers 12     # 重建索引
py -3.12 "BASS DR3\tools\build_coords_index.py"              # 重建坐标索引 (已下载过 ccdinfo 则直接解析)
py -3.12 "BASS DR3\tools\verify_bass_dr3.py" --sample 30     # 全量一致性 + 随机下载核对
```

## 11. 已知限制

- 索引基于镜像站 HTML 列表（2026-08-12 抓取），服务端未提供文件修改时间。
- 部分路径 HTTP `HEAD` 返回 404，但 `GET`/Range `GET` 正常；校验脚本用 Range GET 核对字节数。
- `od` 命名含义为论文 + 文件头推断（详见第 2 节）。
- ccdinfo 部分行缺测量值（见第 3 节），使用前过滤。
- 归档存在跨日期重复条目（见第 5 节），取数据时认准正确日期目录。
- 本目录已加入根 `.gitignore`（`BASS DR3/`），为独立数据工作区，不进入主线仓库。
