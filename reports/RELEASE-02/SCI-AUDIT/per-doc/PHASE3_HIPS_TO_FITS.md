# per-doc: docs/science/PHASE3_HIPS_TO_FITS.md（SCI-P3-001，FROZEN；RELEASE-02 已订正）

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-CC-04 | 32,147-148（订正后） | variance/ivar 子产品必须显式消费传播；weight/support 仍拒绝 | 现文已同步 DATA-UNC-001 更新块，同合同互斥消除 | CORRECT | — |
| R2-H-27 | 150 | Fernique 2015 HiPS A&A 578 A114 | Crossref 10.1051/0004-6361/201526075 ✅ | CORRECT | — |
| R2-K-01 | §9a-7 | 采样核 nearest|bilinear，默认 bilinear | 与 V6 kernel registry（bilinear_4quad / bilinear_area_overlap_exact，生产默认后者）不一致 | CONTRADICTS | DESIGN-OK-DOC-WRONG |

| R2-L-01 | 63 | 声称 IVOA hips_frame 值域={icrs,galactic,ecliptic}；写 icrs | **本审计员对 REC-HIPS-1.0-20170519 官方 PDF 逐字抽取**：值域=「equatorial」(ICRS)/「galactic」/「ecliptic」，示例=equatorial；icrs 非法 | **WRONG**（标准符合性） | DESIGN-OK-DOC-WRONG |
| R2-L-02 | 126,151 | bilinear 误差 O(h²) | P1-B 四象限方案实测 O(h)（max_err/h=0.65 常数）；但生产默认核是 bilinear_area_overlap_exact，结论依赖 U-K 词表 | AMBIGUOUS | DESIGN-OK-DOC-WRONG |

## 处置建议

- 采样核词表与生产默认统一（见 REGISTER R2-K-01）。
- 其余合同已自洽；Phase3 alpha 未实现，本合同为施工边界。
