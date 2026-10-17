# 球面与导出算法

[HEALPix](HEALPIX_MAPPING.md)与[Drizzle 几何](DRIZZLE_GEOMETRY.md)补充球面坐标和交叠，[重采样](PHASE3_RESAMPLE.md)补充平面取样。目录内仍有[投影](PHASE3_PROJ_IMPL.md)、[重采样实现](PHASE3_RSMP_IMPL.md)、[FITS写出](PHASE3_FITS_IMPL.md)和[HiPS writer](HIPS_WRITER.md)的混合实现资料，当前调用以[Export 模块](../../../detail/export/modules.md)为入口；这些资料的全文拆分继续按原始清单记录。


**D7-A 已逐篇重构**：[P3 Projection/WCS](PHASE3_PROJ_IMPL.md) 保留 G1/G2/registry 的现有公式，补真实 TAN-only CLI、CRPIX/CD、crop、#79/#88/#89；[P1 Drizzle 曲边](DRIZZLE_GEOMETRY.md) 保留 P1 数值核，补 #15 的真实 HEALPix 叶/独立面积 Oracle 与受控自适应方案。该文件虽然在 export 目录，但科学职责是 Stage1 forward spherical overlap，区别于 Phase3 bilinear。正式科学正本为 [Drizzle](../../drizzle/DRIZZLE.md) 与 [P3 HiPS→FITS](../../projection/HIPS_TO_FITS.md)，模块接口见 [Export detail](../../../detail/export/modules.md)。
