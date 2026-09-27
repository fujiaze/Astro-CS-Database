# research

本目录存放科学与格式选题的研究包：一手文献、开源实现对照与核验留痕，为科学断言提供佐证基础。

## 职责边界

- 放：研究包正文（一手出处、方法学对照、核验标注）与机器生成的基准数据表。
- 不放：公式、常数与门限的定义（在 docs/science/）；文献总清单（在 docs/references/）；实验单元（在 实验/ 目录）。
- 约束：研究包只承载佐证，不定义公式、常数与门限。

## 内容

- `PHOTOMETRY_RESEARCH_PACK.md` —— 测光标定研究包：Gaia XP 与系统响应到测光星等坐标系的文献与开源对照。
- `SNR_WEIGHT_RESEARCH_PACK.md` —— 帧级 SNR、PSF 权重与逆方差叠加的研究包。
- `IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md` —— IVOA HiPS 标准瓦片格式条款的一手查证。
- `COMPRESSION_CODEC_RESEARCH_PACK.md` —— 压缩编码（zstd/Rice/TRIM/FITS tile-compression）评估研究包。
- `COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md` —— 压缩基准原始数字汇总表（机器生成）。
- `CCD_LINEAR_DEFECT_LITERATURE.md` —— CCD/CMOS 线性缺陷（坏列/坏行/拖尾列）检测与修复的一手文献证据。

## 上游

上游：docs/ASTROCS_DESIGN.md §2.1–§2.2（创新点一、二）、§4.4（输出合同）、§8.3（异步、预取与压缩）、§9（内存极简化）、§10（I/O 边界）、附录 B（外部标准与文献）。

科学断言以 docs/science/ 为权威，佐证要求见 docs/DOCUMENT_GOVERNANCE.md §2。
