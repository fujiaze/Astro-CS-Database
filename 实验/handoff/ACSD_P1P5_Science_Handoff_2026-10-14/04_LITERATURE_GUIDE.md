# 五篇论文共享参考资料（学术主干）

正式机器书目：`实验/shared/references/CORE_REFERENCES_VERIFIED.bib`；最新核验表：`CORE_REFERENCES_VERIFIED.md`；每篇具体“前例/可主张差异/无证据”见 `PRIOR_ART_CLAIM_MATRIX.md`。全部在仓库中，不能在此离线副本之外重新建立可修改的独立引文编号系统。

| ID | 作者与年份 | 核验 DOI / 官方入口 | 归属与注意 |
|---|---|---|---|
| R01 | Gaia Collaboration / Montegriffo et al. 2023 | https://doi.org/10.1051/0004-6361/202243709 | P1 Gaia XP 合成光度先例 |
| R02 | Xiao et al. 2025, Mini-SiTian | https://doi.org/10.3847/2041-8213/adbd3c | P1 CMOS 光度标定直接先例 |
| R03 | Padmanabhan et al. 2008 | https://doi.org/10.1086/524677 | P1/P5 全球相对光度规范与自由度 |
| R04 | Newberry 1991 | https://doi.org/10.1086/132801 | P2 扣背景 CCD 噪声 |
| R05 | Zackay & Ofek 2017 | https://doi.org/10.3847/1538-4357/836/2/188 | P2/P4 proper coadd，注意标题确为 COAAD |
| R06 | Fruchter & Hook 2002 | https://doi.org/10.1086/338393 | P3 Drizzle 数学与相关噪声 |
| R07 | Górski et al. 2005 | https://doi.org/10.1086/427976 | P3 HEALPix 像素框架 |
| R08 | Wahba 1990 | https://doi.org/10.1137/1.9781611970128 | P4 样条与正则 |
| R09 | Hyman 1983 | https://doi.org/10.1137/0904045 | P4 单调保持三次 Hermite 不等同自然样条 |
| R10 | Huber 1964 | https://doi.org/10.1214/aoms/1177703732 | P1/P5 稳健估计 |
| R11 | John Murphy NSG / PMM | https://www.astroprocessing.com/nsg.html 及 https://www.astroprocessing.com/ | P5 官网算法说明，非全文数理论文；PMM 内部方程不能猜 |
| R12 | PixInsight LocalNormalization 实例 | https://www.pixinsight.com/examples/M31-Ha/ | P5 官方软件流程对照，不等于 PMM 公式 |
| R13 | Horne 1986 | https://doi.org/10.1086/131801 | P2 经典 CCD 最优抽取，源问题是光谱 |
| R14 | Fritsch & Carlson 1980 | https://doi.org/10.1137/0717021 | P4 形状保持三次插值一手数学先例 |
| R15 | Makovoz, Khan & Moshir 2005 | https://doi.org/10.1086/428409 | P5 全局重叠背景 offset 拟合直接先例 |

**关键论文公平性：** P1 的 Gaia XP 不是首创，P3 的 Drizzle/HEALPix 不是首创，P4 的样条/IDW 不是首创，P5 的全局背景偏移拟合不是首创。只有在同一真值与验收口径下，证明 ACSD 具体算法/应用实现相对已有工作有可靠增量才能说“新贡献”。目前不应声称五篇皆创新已证实，也不应仅凭核心方法有前例就说不可能发表。

**后续文献精读约束：** 只能对拿到完整原文的具体页/式号登记 MATH_CHECKED。对书本级引用没核正文就标 BIB_VERIFIED；禁止编造 page、formula number、源码 API。发现文献库错项直接在远端原文件修订，附 DOI/出版社链和本次检查说明。
