# Astro Celestial Sphere Database（ACSD）

ACSD 是一个天文 CCD/CMOS 图像校准与标准化数据库：把单帧天文观测转换为可独立消费、带不确定度与
来源链的球面科学产品（HiPS），再按明确科学目标合成马赛克，或导出测量意义明确的平面 WCS FITS。

科学核心是一条链：先把每帧校准到统一的测光星等坐标系（以 Gaia DR3 XP 星表正向合成期望测光量、
拟合逐帧乘性标度），在这个坐标系上测量不受天光影响的绝对信噪比，再用加性方式去除天光、建立帧间
连续的绝对信号平面，使叠加结果天然无接缝；配合全天 HEALPix 产品上的精确面积交叠分配完成通量守恒
重采样。正式平台 Windows x64 与 Linux amd64，纯 CPU 生产。

## 构建

需要 CMake、Ninja 与 C++ 工具链（依赖与工具链冻结值见 `docs/engineering/DEPENDENCY_RULES.md` 与 `docs/engineering/TOOLCHAIN_AGENT_HOST.md`）。

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # 仓库唯一的根 CMake
ninja -C build
ctest --test-dir build --output-on-failure                # 测试
python3 eng/ci/run_checks.py                              # 机器一致性检查（注册表 eng/ci/checks.json）
```

## 使用

对外只有一个 CLI 入口 `acsd`，三个命令平级独立：独立启动、独立重跑、独立验收。

```text
acsd normalize --json <config.json>     # 单帧标准化：light + 校准帧 + 滤镜 → 标准化 HiPS（帧级 SNR 入头）
acsd mosaic    --json <config.json>     # 马赛克：合同兼容 HiPS 组 → 马赛克 HiPS + provenance
acsd export    --json <config.json>     # 投影导出：任一合同兼容 HiPS → 平面 WCS FITS
acsd help
acsd doctor [--json]                    # 环境自检
acsd benchmark                          # 生成/更新安装目录 cpu_profile（后续运行自动读取）
```

配置不用手写：`acsd <命令> --template [-o <path>]` 生成可改的完整 JSON 模板，`--help` 给字段说明。
运行前有配置预检三档（correct / warn / error，error 阻塞），事件流是默认输出（stdout 每行一个 JSON
事件，GUI 可直接捕获）。退出码全 11 条冻结（0 成功 … 10 磁盘写满、70 未分类内部错误），唯一源
`lib/infrastructure/cli/exit_codes.h`；完整协议见 `docs/engineering/CLI_PROTOCOL_V1.md`。

## 文档与权威链

权威链只有一条，自上而下递减：与下级文档冲突时以 `docs/ACSD_DESIGN.md` 为准。全文档集唯一索引
= `docs/DOCUMENT_INDEX.yaml`，各目录均有中文 `README.md` 说明职责与内容。

| 入口 | 回答什么 |
|---|---|
| [`docs/ACSD_DESIGN.md`](docs/ACSD_DESIGN.md) | 是什么、做到什么、顶层架构、CLI、发行、验收（最高设计） |
| [`AGENTS.md`](AGENTS.md) | 干活纪律与工作流（开工前逐层读文档、硬禁令、自查自修） |
| [`docs/engineering/DOCUMENT_GOVERNANCE.md`](docs/engineering/DOCUMENT_GOVERNANCE.md) | 文档分层、准入、写法与登记判据 |
| [`docs/engineering/EXECUTION_MODEL.md`](docs/engineering/EXECUTION_MODEL.md) | 工作包的制作与执行纪律、收口即清理 |
| [`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md`](docs/engineering/VALIDATION_EVIDENCE_STANDARD.md) | 独立 Oracle、零用例即红、四层验收判据 |
| [`docs/engineering/CI_SPEC.md`](docs/engineering/CI_SPEC.md) | 机器门怎么跑、证据落哪 |
| [`docs/detail/00_INDEX.md`](docs/detail/00_INDEX.md) | 逐模块工作细节 |

与上述入口并列的下级权威：`docs/science/`（科学公式与定义式）、`docs/science/algorithms/`（算法推导与符号表）、
`docs/detail/UNIFIED_MODEL.md`（数据对象与三类配置）、`docs/contracts/`（合同说明，与 `eng/contracts/`
机器 schema 双向对应）；科学佐证纪律见 `docs/engineering/DOCUMENT_GOVERNANCE.md`。

| 要读什么 | 去哪读 |
|---|---|
| 数据对象、三类配置、逐阶段详细设计 | `docs/detail/UNIFIED_MODEL.md`、`docs/detail/PHASE{1,2,3}_DETAILED_DESIGN.md` |
| 科学定义式、单位、适用域 | `docs/science/` |
| 算法推导、符号表、算法级边界 | `docs/science/algorithms/` |
| 产品字段、键集、值域（文档侧） | `docs/contracts/` |
| 机器校验 schema（机器侧唯一事实源） | `eng/contracts/` |
| 模块工作细节 | `docs/detail/`、`docs/modules/` |
| 架构与不变量 | `docs/architecture/` |
| 跨阶段产品交换与 ABI | `docs/engineering/io/`、`docs/science/IO_001_FITS_STREAM_INTERFACE.md`、`docs/science/IO_002_HIPS_INPUT_INTERFACE.md` |
| CLI/API 协议 | `docs/engineering/CLI_PROTOCOL_V1.md` |
| 机器门清单与运行方式 | `docs/engineering/CI_SPEC.md`、`docs/engineering/01_CHECKS.md` |
| 验收证据与 QA 矩阵 | `docs/engineering/v6/QA_MATRIX.md`、`实验/engineering-evidence/` |
| 术语 | `docs/GLOSSARY.md` |
| 开发与排查 | `docs/engineering/DEVELOPER_GUIDE.md`、`docs/detail/merged_TROUBLESHOOTING.md` |

## 仓库布局

算法在 `lib/algorithms/`（并联放置），基建与 CLI 在 `lib/infrastructure/`，工程支撑面在 `eng/`
（合同 schema、机器门、构建与质量工具、程序全局配置），自解释文档集在 `docs/`；科学实验单元在
`实验/`，证据在 `artifacts/`，过程产物在 `run/`（不入库）。
模块索引权威 = `docs/engineering/MODULE_MAP.md`；未决事项见 `docs/engineering/UNRESOLVED_REGISTER.md`。

## 状态与版本

状态词表唯一口径 = `docs/ACSD_DESIGN.md` §12.5；逐模块状态与证据锚见 `docs/engineering/RELEASE_STATUS.md`
与 `docs/modules/MODULE_MAP.yaml`。产品版本唯一事实源 = 仓库根 `VERSION`，CMake、CLI、产品 manifest
由该源派生。已知限制台账：`docs/KNOWN_LIMITATIONS.md`。

## 参考项目与文献

### 文献

**Drizzle 与球面重采样**
- Drizzle 算法（Paper I）：Fruchter & Hook 2002，[arXiv:astro-ph/0207407](https://arxiv.org/abs/astro-ph/0207407)、[DOI 10.1051/0004-6361:20021326](https://doi.org/10.1051/0004-6361:20021326)
- Drizzle 配套论文（Paper II）：[DOI 10.1051/0004-6361:20021327](https://doi.org/10.1051/0004-6361:20021327)
- SIP 多项式畸变表示：Shupe et al. 2005, ASPC 347, 491
- HEALPix：Górski et al. 2005, ApJ 622, 759，[DOI 10.1086/427976](https://doi.org/10.1086/427976)

**天体测量与 platesolve**
- FOCAS 三角匹配：Valdes et al. 1995, PASP 107, 1119，[DOI 10.1086/133667](https://doi.org/10.1086/133667)
- 平面星表模式匹配：Groth 1986, AJ 91, 280，[DOI 10.1086/114099](https://doi.org/10.1086/114099)
- astrometry.net：Lang et al. 2010, AJ 139, 1782，[DOI 10.1088/0004-6256/139/5/1782](https://doi.org/10.1088/0004-6256/139/5/1782)、[arXiv:0910.2233](https://arxiv.org/abs/0910.2233)
- SCAMP 天测标定：Bertin 2006, ASPC 351, 112
- k-vector 范围搜索：Mortari 1999, J. Astronaut. Sci.（候选出处，佐证充实中）

**检测与测光**
- SourceExtractor：Bertin & Arnouts 1996, A&AS 117, 393，[DOI 10.1051/aas:1996164](https://doi.org/10.1051/aas:1996164)
- DAOPHOT：Stetson 1987, PASP 99, 191，[DOI 10.1086/131977](https://doi.org/10.1086/131977)

- Schechter, Mateo & Saha 1993（CCD 增益与读噪声标定），[DOI 10.1086/133316](https://doi.org/10.1086/133316)
- Horne 1986（最优提取），[DOI 10.1086/131801](https://doi.org/10.1086/131801)
- Naylor 1998（加权测光与不确定度），[DOI 10.1046/j.1365-8711.1998.01314.x](https://doi.org/10.1046/j.1365-8711.1998.01314.x)
- Zackay & Ofek 2017（How to coadd images? I/II），[arXiv:1512.06872](https://arxiv.org/abs/1512.06872)、[arXiv:1512.06879](https://arxiv.org/abs/1512.06879)
- Mighell 1998（星像 peaker 谱系，KPNO）
- Siril：[arXiv:2408.03346](https://arxiv.org/abs/2408.03346)

**校准与宇宙线**
- L.A.Cosmic：van Dokkum 2001, PASP 113, 1420，[arXiv:astro-ph/0108003](https://arxiv.org/abs/astro-ph/0108003)
- 平场适用性检验：Marshall & DePoy 2005，[arXiv:astro-ph/0510233](https://arxiv.org/abs/astro-ph/0510233)
- 暗场-曝光稳健线性回归与热像素：Hochedez et al. 2013，[arXiv:1303.1437](https://arxiv.org/abs/1303.1437)

**统计与稳健估计**
- Huber 1964（M 估计），[DOI 10.1214/aoms/1177703732](https://doi.org/10.1214/aoms/1177703732)
- Holland & Welsch 1977（IWLS 稳健回归），[DOI 10.1080/00401706.1977.10489534](https://doi.org/10.1080/00401706.1977.10489534)
- Rousseeuw & Croux 1993（Qn 稳健尺度），[DOI 10.1080/01621459.1993.10476308](https://doi.org/10.1080/01621459.1993.10476308)
- 中位数/MAD 数值口径：Akinshin 2022，[arXiv:2207.12005](https://arxiv.org/abs/2207.12005)
- 求和数值误差：Baumer 2017，[arXiv:1706.07400](https://arxiv.org/abs/1706.07400)
- Ipatov 2006，[arXiv:astro-ph/0610931](https://arxiv.org/abs/astro-ph/0610931)

- Clopper & Pearson 1934（二项置信区间），[DOI 10.1093/biomet/26.4.404](https://doi.org/10.1093/biomet/26.4.404)
- Young & van Vliet 1995（递归高斯滤波），[DOI 10.1016/0165-1684(95)00020-E](https://doi.org/10.1016/0165-1684(95)00020-E)
- Levenberg 1944 / Marquardt 1963 / Moré 1978（LM 优化族）

**PROSAC 采样**：Chum & Matas 2005，[DOI 10.1109/CVPR.2005.221](https://doi.org/10.1109/CVPR.2005.221)

**星表**：Gaia DR3，Gaia Collaboration 2023，[arXiv:2208.00211](https://arxiv.org/abs/2208.00211)

**Astropy 社区**：Astropy Collaboration 2013/2018/2022，[DOI 10.3847/1538-3881/aabc4f](https://doi.org/10.3847/1538-3881/aabc4f)

### 开源项目

| 项目 | 许可 | 关联 |
|---|---|---|
| [Siril](https://gitlab.com/free-astro/siril) | GPL-3.0 | 校准/cosmetic/platesolve 对照实现；星检测算法逻辑学习参考（仅学习其算法逻辑，未直接使用其代码） |
| [SourceExtractor](https://github.com/astromatic/sextractor) | LGPL-3.0 | 星检测与测光基准 |
| [SWarp](https://github.com/astromatic/swarp) | GPL-3.0 | 重采样与叠加语义锚 |
| [SCAMP](https://github.com/astromatic/scamp) | GPL-3.0 | 天测标定对照 |
| [astrometry.net](https://github.com/dstndstn/astrometry.net) | GPL-3.0-or-later | platesolve 受限求解与 verify |
| [astropy](https://github.com/astropy/astropy) | BSD-3 | WCS/SIP 参考实现 |
| [ccdproc](https://github.com/astropy/ccdproc) | BSD-3 | 图像校准流程对照 |
| [astroscrappy](https://github.com/astropy/astroscrappy) | BSD-3 | L.A.Cosmic 移植（宇宙线 oracle 候选） |
| [photutils](https://github.com/astropy/photutils) | BSD-3 | 孔径/PSF 测光对照 |
| [IRAF/NOAO ccdred + DAOPHOT/DAOFIND](https://github.com/IRAF-community/iraf) | 非 OSI | 校准组合与星检测参数锚（ccdmask/zerocombine/darkcombine/findpars） |
| [DoPHOT 镜像](https://github.com/) | 存疑（仅对照，不派生） | PSF 测光谱系对照 |
| [LSST ip_isr](https://github.com/lsst/ip_isr) | GPL-3.0 | ISR 与方差传播对照 |
| [hstcal (calacs)](https://github.com/spacetelescope/hstcal) | BSD-3 | HST 校准链对照 |
| [WCSLIB](https://www.atnf.csiro.au/people/mcalabre/WCS/) | LGPL-3.0 | WCS 参考实现 |
| [Gnuastro](https://www.gnu.org/software/gnuastro/) | GPL-3.0 | 掩膜位语义与统计工具对照 |
| [ESO CPL/pipelines](https://www.eso.org/sci/software/cpl/) | GPL-2.0+ | ESO 流水线族对照 |
| [SDSS](https://github.com/sdss) | 非 OSI | flags 与测光管线对照 |
| [HEALPix](https://healpix.sourceforge.io/) | GPL-2.0+ | 球面像素化方案 |
| [CFITSIO](https://heasarc.gsfc.nasa.gov/fitsio/) | 随库条款 | FITS I/O（随仓 third_party） |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | JSON（随仓 third_party） |

> 完整佐证映射（哪篇支撑我们哪条断言）见 `docs/engineering/SCIENTIFIC_REFERENCES.md` 与 `实验/` 各单元的佐证来源区。
