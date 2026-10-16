# HEALPix NESTED 坐标与层级

本页解释离散索引、连续方向和 HiPS tile 排列的关系，依据[Drizzle 科学](../../drizzle/DRIZZLE.md)与[数据语义](../../unified/DATA_SEMANTICS.md)。共享实现位于 `lib/algorithms/shared/healpix/healpix_core.h/.cpp`；代码的边界分支与目标数学域分别核对。

## 1. 方向、索引与层级

输入方向为 ICRS RA/Dec，接口单位度；内部球面几何转换到弧度。RA 归一到[0,360)，Dec 要求[−90,90]，有限性单独检查。NESTED 使用 nside=2^k，共 `12 nside²` 个叶，ipix 从0起。ACSD 支持域按最高阶与整数容量共同限制，不能只因 nside 是2的幂就认为任意阶都安全。

父子层级差为 shift 时，父索引是 `ipix >> (2 shift)`，首子索引是 `ipix << (2 shift)`；操作前检查阶、索引范围、移位宽度和溢出。tile order 与 leaf order 具有不同意义，512宽 tile 的叶阶比 tile 阶高9。

局部 NESTED 位交错采用 x占偶数位、y占奇数位；HiPS FITS 行主序映射为 `index=(W−1−x) W+y`，当前 W=512。此方向约定须与独立 HiPS 生产者核对，不能仅凭内部正反映射一致证明外部手性正确。

## 2. 各种往返检验

| 检验 | 比较对象 | 解释 |
|---|---|---|
| index→中心→index | 同阶离散 ipix | 检查中心与所属索引一致 |
| 方向→index→中心 | 任意方向与所属叶中心 | 含离散化位移，不能按连续逆函数误差解释 |
| xy/local/FITS→逆映射 | 同一 tile 的离散像素 | 检查排列、手性和整数互逆 |
| 父→子/子→父 | 明确阶差的层级 | 检查编码范围及祖先一致性 |

旧文同时写“round-trip≤1e−12度”和按像元尺度的 `1.2 hp_res+1e−9` 包络，没有充分区分上述对象；记录为[Q25](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。本轮不选择新的通用误差界，旧数值与声明全部保留于[来源档案](../../../../实验/documentation-refactor/docs/SPATIAL_NOTES_SOURCE.txt)。

## 3. 实际入口与错误

共享入口为 ang2pix_nest、pix2ang_nest、nested_local_to_xy、xy_to_nested_local、nested_local_to_fits_index、fits_index_to_nested_local、parent_nest、child_nest 及层级包装。科学算子通过这些公开接口复用，不另抄一份位操作或角距内核。

头文件说明非法 nside 抛 invalid_argument，部分移位溢出抛 overflow_error；pix2ang_nest 对越界 ipix 当前返回RA=Dec=0，不能把该返回当真实天区。order_to_nside、leaf_to_tile_nest 等薄包装也需调用者先保证合法阶，不能概括为所有非法输入都已 checked。异常在外层 C ABI 转换为错误，不跨模块边界。

## 4. 独立比较与证据

覆盖12基面、极区、赤道转折、RA回绕、边界及各阶，比较独立实现的索引集合与 tile 排列。有限随机点无 mismatch 只支持所测集合，不能证明所有边界和查询候选完备。几何交叠还需真实曲边 oracle，索引一致不证明 Drizzle 面积或通量正确。

头注释保存百万点和 order0..22 的历史对照声明。旧文列 `tests/test_healpix_oracle.cpp`、neighbors/tile-mapping 等载体，本轮未在当前目录取得其中指定 oracle 文件；这些名称不能继续作为现有可运行测试的证明。原始输入、独立生成器版本、命令、分母与结果需回到所属实验查证。

Górski et al. 2005（DOI 10.1086/427976）、IVOA HiPS 与 Fernique et al. 2015 提供理论/格式阅读来源；独立实现与许可证按实际版本核验，见[HiPS 科学分册](../../formats/HIPS_TILE_FORMAT.md)。本轮未重新联网验证全部文献或运行坐标测试。
