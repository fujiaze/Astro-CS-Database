# 球面与导出算法

[HEALPix](HEALPIX_MAPPING.md)与[Drizzle 几何](DRIZZLE_GEOMETRY.md)补充球面坐标和交叠，[重采样](PHASE3_RESAMPLE.md)补充平面取样。目录内仍有[投影](PHASE3_PROJ_IMPL.md)、[重采样实现](PHASE3_RSMP_IMPL.md)、[FITS写出](PHASE3_FITS_IMPL.md)和[HiPS writer](HIPS_WRITER.md)的混合实现资料，当前调用以[Export 模块](../../../detail/export/modules.md)为入口；这些资料的全文拆分继续按原始清单记录。


**D7-A 已逐篇重构**：[P3 Projection/WCS](PHASE3_PROJ_IMPL.md) 保留 G1/G2/registry 的现有公式，补真实 TAN-only CLI、CRPIX/CD、crop、#79/#88/#89；[P1 Drizzle 曲边](DRIZZLE_GEOMETRY.md) 保留 P1 数值核，补 #15 的真实 HEALPix 叶/独立面积 Oracle 与受控自适应方案。该文件虽然在 export 目录，但科学职责是 Stage1 forward spherical overlap，区别于 Phase3 bilinear。正式科学正本为 [Drizzle](../../drizzle/DRIZZLE.md) 与 [P3 HiPS→FITS](../../projection/HIPS_TO_FITS.md)，模块接口见 [Export detail](../../../detail/export/modules.md)。


**D7-B 两篇原始长文已经正式精读与改写**：[Stage3 HiPS Resample](PHASE3_RSMP_IMPL.md)（NESTED/标准目录与历史fallback、实际 bilinear、#88/#89、#90 一次再现、负缓存和当前 Runtime双平面）与 [Stage3 FITS Writer](PHASE3_FITS_IMPL.md)（单 PRIMARY HDU、stream状态机、发布前checksum/sha、裁剪、BUNIT/响应、读回校验状态）。P1真实球面 Drizzle仍由 D7-A [Drizzle](DRIZZLE_GEOMETRY.md)独立负责；当前 Stage3 bilinear不是P1 leaf曲边交叠。本批没有执行 D7-C。


**D7-C 结题的原长文**：[G1–G5 Stage3 Resample](PHASE3_RESAMPLE.md)与[HEALPix HiPS Writer](HIPS_WRITER.md)已对63eccbb6原始全文、main真实C++接口逐一复核。前者强调#86“选择的order不是真正读取的order”、#90非矩形bilinear/#94校验状态；后者强调P1 Drizzle的F/N与AIO的F'/D换算、f64稀疏层级、#9/#10/#13/#15/#19/#74/#75和单文件原子相对于完整P1科学revision的边界。原历史设计和研究性测试继续保留正文，实验未运行不标PASS；此批是文档语义收敛，不代表整仓科学闭合。

