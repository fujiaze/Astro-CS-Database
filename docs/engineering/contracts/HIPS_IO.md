# HiPS 输入读取

本合同说明磁盘HiPS的属性、tile寻址、缺块、类型与读取边界。几何和单位由[HiPS科学分册](../../science/formats/HIPS_TILE_FORMAT.md)定义，目标存储形态见[存储合同](HIPS_STORAGE_FORM.md)，实际调用见[AIO细节](../../detail/infrastructure/aio.md)。

## 1. 实际入口与支持域

`lib/infrastructure/aio/include/aio_hips.h`及其reader是三阶段和浏览器的实际读取面；`io/include/acsd/io/hips_input_v1.h`另提供版本化C接口，复用fits_stream_v1。声明接口和生产调用分别核对，不把所有reader当成一个实现。

项目科学输入使用NESTED、天球参考系、float FITS信号。低层可读取的support/variance/ivar是文件能力，当前Mosaic最终只发布signal；其他reader支持范围和阶段产品集合分别说明。整数/JPEG/PNG或其他类型是否接受按具体科学调用者处理，不由格式词表存在自动授权。

## 2. 属性与地址

读取properties的dataproduct_type、hips_order、hips_tile_width、hips_frame、hips_tile_format及必要单位/来源，核对该reader的域和实际tile。属性别名按显式兼容规则解释，未知值明确返回不支持。

tile地址为`NorderK/DirD/NpixN.fits`，标准块目录按完整ipix的万进制起点；既有商余命名兼容只能在具名读分支使用。检查order、ipix整数容量、tile宽和shape，再读数据；不能仅凭文件名判断天区、叶阶或坐标手性。512宽tile的叶阶与tile阶差9，使用共享NESTED/FITS映射。

## 3. 状态与缺覆盖

tile定位分PRESENT、MISSING、INVALID；MISSING表示该请求层没有块，不能静默返回父层内容冒充。上层如有显式选择其他层级的算法，记录真实层级与算子，和底层缺块错误分开。

局部树允许缺子树；无覆盖signal按NaN/明确状态，不能填0。tile内合法非有限表示、数据损坏和读取错误按接口策略区分。Moc.fits可作可选枚举提示，损坏提示不应制造伪覆盖；查询候选和实际有效像元不同。

### 3.1 tile 读取完整性分类（Issue #87，生产合同）

reader 的整 tile 读取入口 `aio_hips_read_tile_f32/f64/i32_ex` 返回稳定分类，旧 `aio_hips_read_tile_*` 保留 `0=成功/非0=失败` 与旧负码兼容：

| code | 语义 | 生产处置 |
|---|---|---|
| `OUTSIDE_DECLARED_FOOTPRINT` | 证明确实没有覆盖 | 合法：NaN / coverage=0，可进 absent 负缓存 |
| `DECLARED_MISSING` | 覆盖声明内 tile 缺失 | 输入完整性错误，fail-closed |
| `IO_ERROR` | 真实打开/读取/权限/IO 故障 | fail-closed，CLI I/O 域 |
| `INVALID_TILE` | FITS 截断/NAXIS/尺寸/BITPIX/头部损坏 | fail-closed，输入域 |
| `CHECKSUM_FAILED` | 存在 DATASUM/CHECKSUM 且验证失败 | fail-closed，输入域 |
| `FOOTPRINT_UNDETERMINED` | 覆盖声明存在但身份不足以判定缺失是否合法 | fail-closed，不冒充合法空洞 |

覆盖身份按强弱分级并随成功产物显式落盘：`MOC_LEAF`（MOC 完整枚举 `order==hips_order` 叶级单元）→ 集合内缺失 `DECLARED_MISSING`、集合外缺失 `OUTSIDE_DECLARED_FOOTPRINT`；`MOC_UNDETERMINED`（MOC 含低阶父级/非法阶条目，或 `manifest.n_leaf_tiles` 大于 MOC 叶级条目）→ 集合外缺失 `FOOTPRINT_UNDETERMINED`（父级覆盖归约是独立任务 #95，本 reader 不静默归约）；`MANIFEST_ONLY`（无 MOC 但 manifest 声称有覆盖）→ 缺失 `FOOTPRINT_UNDETERMINED`；`NONE`（无 MOC 且无覆盖计数）按“MOC 是可选枚举提示”把未声明单元的缺失记为 `OUTSIDE_DECLARED_FOOTPRINT`，身份等级随产物显式记录，不暗中冒充 `MOC_LEAF`。

校验规则：DATASUM/CHECKSUM 存在时以已读字节计算 1's-complement 校验和（不额外重读 1 MiB 数据）；缺失校验字段不是失败，但必须在缓存统计 `checksum_absent_reads` 中显式计数；BSCALE/BZERO/BLANK/压缩等改变原始字节语义的路径回退 cfitsio 全 HDU 校验。表观 absent 与完整性错误必须在 tile cache 中**分开存储**：错误缓存命中复现同一 code/path/CFITSIO 状态，不会降级成“缺失”。多 worker 聚合按 `tile_ipix` 升序、同一 tile 合并一条并记录 `worker_min/max/count`；失败运行不得发布 FITS、resampled bin 或 complete manifest。

## 4. 句柄与缓冲

open解析产品/子产品并校验属性，调用方分配输出缓冲，使用完close。单句柄串行，不同worker各有reader或在读层串行化共享句柄。dtype转换、缓冲容量、部分读和取消采用对应FITS接口合同，不能把旧整数码按CLI退出码直接解释。

读取后保持真实单位、scale_state、运行/frame身份及必要模型引用。兼容外部HiPS可按信号域读取，缺canonical precision明确unavailable，不要求所有输入由ACSD某个Store生成。

## 5. 裸形态与归档目标

设计支持裸`.hips/`与带产品索引的`.hips.zst`统一读语义：归档按成员索引定位独立压缩帧，查询覆盖不解压整包。归档索引缺失或损坏须明确报错，裸目录的枚举回落另记来源。

当前生产归档写/读、产品索引和storage事实接线未由该接口声明证明，实际裸读路径与未实现目标分开，见[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。不能让未实现默认archive的文档描述使用户以为已可读取。

验证properties非法、层级/宽度、地址越界、缺块、父回退、手性、dtype、取消、归档索引及兼容外部产品。标准符合性、当前代码支持与真实运行分别记录；原文保存于[来源档案](../../../实验/documentation-refactor/docs/IO_SOURCE.txt)。
