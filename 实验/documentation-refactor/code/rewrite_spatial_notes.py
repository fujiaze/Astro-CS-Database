"""Preserve spatial/PSF notes and separate scientific explanation from interfaces."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
    'docs/science/algorithms/export/HEALPIX_MAPPING.md': r'''# HEALPix NESTED 坐标与层级

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
''',
    'docs/science/algorithms/normalize/STAR_PSF_ALGORITHMS.md': r'''# Moffat4 拟合的数值方法

本页补充[PSF 科学](../../psf/PSF.md)的参数化、数值求解与误差解释。[模块正文](../../../detail/normalize/modules.md#star-psf)拥有数组布局、内存与调用；[Normalize 数据](../../../engineering/contracts/NORMALIZE_DATA.md)规定完整参考对象。当前逐星拟合、像元积分 evaluator 与空间 PSF 是三种不同责任。

## 1. 参数化与积分量

当前拟合使用 β=4 的椭圆 Moffat：

\[
 I(x,y)=B+\frac{A}{(1+Q)^4},\qquad
 Q=p_1 d_x^2+2p_2d_xd_y+p_3d_y^2,
\]
\[
 p_1=\frac{\cos^2\theta}{2s_x^2}+\frac{\sin^2\theta}{2s_y^2},\quad
 p_2=\frac{\sin2\theta}{4s_x^2}-\frac{\sin2\theta}{4s_y^2},\quad
 p_3=\frac{\sin^2\theta}{2s_x^2}+\frac{\cos^2\theta}{2s_y^2}.
\]

B、A 与输入图像同标度；中心和宽度为像元，θ为弧度。圆形时 Q=r²/(2σ²)，各轴 FWHM 为

\[
 2\sqrt2\,s\sqrt{2^{1/4}-1}=1.230307652590102\,s.
\]

实现常量1.230310是该关系的近似。整平面拟合轮廓的解析总通量为 `2π A sx sy/3`；它是该连续模型的积分，不自动证明中心采样拟合等于像元积分测光。圆形轮廓窗外份额为 `(1+r_win²/(2σ²))^(−3)`；窗口截断的模型份额与真实源失配误差分别评价。

完整参考 PSF 的总通量、空间 alpha 与球面 R 必须按[Normalize 科学链](../../normalize/NORMALIZE_SCIENCE.md)配对。不能把求解窗重新归一并仍称同一个总通量，也不能用拟合解析 flux 取代 canonical reference 身份。

## 2. 当前逐星数值步骤

从指定拟合窗收集有限像元，以背景初值、峰值振幅、中心偏移、轴宽与角度初始化七参数。Levenberg–Marquardt 迭代用有限差分 Jacobian 和阻尼法方程，调整λ、限制宽度并检查停止条件。当前实现的窗口规则、初值、硬编码迭代参数与状态见模块补充，不在科学页重复建立配置默认值。

θ与轴交换存在等价表示；接近圆形时角度本身可弱可辨识。比较按模型响应及适当归约的参数解释，不能把角度相同当全部 PSF 一致。步后硬钳位、病态 Jacobian、有限差分尺度和残差筛选可能影响收敛，须按真实数据域测量，不能把“使用 LM”当无条件收敛保证。

逐星拟合在当前离散像元值上评估轮廓；球面参考路径另调用 `pixel_integrated_moffat4_profile`，空间模型另提供位置求值。求值能力存在不证明上游拟合模型已与像元积分观测一致，相关疑点见[Q08/Q09](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。

## 3. 残差尺度与质量代理

字段 mad 当前实际为10–90%截尾平均绝对残差，记 residual_scale。对独立固定高斯残差，渐近换算比例为

\[
 c=\frac{2[\phi(\Phi^{-1}(0.55))-\phi(\Phi^{-1}(0.95))]}{0.8}
 \simeq0.7316730952806134.
\]

实现使用0.7316727929211932近似常量。有限样本取整、拟合自由度、相关性、异方差和非高斯污染需另评估；不能将该系数当拟合后残差的普适无偏修正。`q_psf=A/residual_scale` 是质量代理，不是积分通量 SNR 或 Fisher 信息。

无残差或数值零残差时，不能由除零制造无限科学精度；状态与接口输出按实际消费者解释。旧有限样本和非高斯读数是特定实验记录，已移至[来源档案](../../../../实验/documentation-refactor/docs/SPATIAL_NOTES_SOURCE.txt)，没有在本轮重跑。

## 4. 独立验证

无噪解析图用于检查模型计算、参数回收和实现布局；像元积分真值与中心采样拟合对照用于暴露模型误差，不能让两侧复制同一采样近似。带噪重复实验报告位置、轴宽、全通量、背景、偏差、散度和区间覆盖，覆盖子像元相位、截断、椭率、空间变化、坏点、饱和与拥挤。

按星 ID 核对 compact 输出与逐星状态，分别验证单星和批接口。串并行和FP32/FP64等价需实际读数，独立星计算不自动证明所有线程配置下逐位一致。旧0.05像元/1%等值仅是具名测试目标，无输入与容差依据时不提升为通用科学误差保证。

方法来源包括 Moffat 1969、Levenberg 1944、Marquardt 1963、Moré 1978 与 Stetson 1987；参考实现可比较 PSFEx、photutils 或独立优化器，实际版本、许可证和采用部分另行核验。文献书目、数值方法、代码存在与真实验证分别报告。
''',
    'docs/detail/infrastructure/gaia_xpsd_client.md': r'''# 本地 Gaia XPSD 查询

本模块离线解析 Gaia DR3/DR3SP 本地 XPSD，供天测与测光使用。查询几何、量化与数值风险见[Gaia 算法](../../science/algorithms/common/GAIA_QUERY.md)，坐标历元见[天体测量](../../science/detection/ASTROMETRY.md)，资源规则见[缓存](../../engineering/standards/CACHE.md)。

## 1. 实际入口与消费者

公开头为 `lib/infrastructure/gaia_xpsd_client/src/gaia_client.h`，实现为同目录 gaia_client.c。`gaia_client_create[_ex]` 建立 client，cone_search、cone_search_for_solver、cone_search_with_spectrum、cone_search_with_photometry 和 query_spectrum_by_coords 分别返回不同字段；destroy 回收 mmap、块缓存与查询缓存。

模块 DLL 的 module_entry.c 经 acsd_module_query_v1 提供描述、配置、规划和执行适配；CLI 构建内调用与该动态入口分别核对，见[模块地图](../../engineering/architecture/MODULE_MAP.md)。IPV 使用求解候选，WCS 后测光使用位置/光谱匹配；查询器不执行板解、SIP或合成测光。

## 2. 输入、输出与单位

| 接口对象 | 合同 |
|---|---|
| data_dir/db_type | 本地目录与 AUTO/DR3/DR3SP；没有网络下载或在线回退 |
| ra/dec/radius_deg | 天球方向与锥半径，度；有限性、范围和RA回绕按入口核对 |
| mag_low/mag_high | G星等窗，mag；量化边界不声称逐位精确 |
| match_radius_arcsec | 按位置光谱匹配半径，角秒 |
| GaiaStar | ra/dec、G/BP/RP、parallax、pmra/pmdec、source_id；ICRS与历元分开解释 |
| 光谱行与字节数组 | 样本 `flux=byte×flux_mul+flux_min`，W m⁻² nm⁻¹；波长由 get_spectrum_params 查询，不硬编码343点 |
| out_match_idx | 与请求位置关联的索引，未匹配与空结果分别处理 |

结果容量和计数与头文件对应。无光谱不等于真实零光谱；DR3字段缺省/哨兵不能按0mag或0误差解释。星表位置的参考历元与观测时刻须在消费者保留，不能因注释写 J2000 就省略自行与历元核对。

## 3. 缓存与并发

查询键包含请求的双精度方向、半径、星等窗及当前数据集相关字段和缓存版本；具体比较方式按 query_cache_lookup 核对，不能仅凭注释称浮点逐位身份。查询缓存和块缓存为进程内存，容量、TTL、共享字节预算由代码配置；不把客户端绑定等同于完整内容哈希身份。

缓存命中在 client 锁内复制结果，成本随返回行数增长。文件分片有各自 collector/scratch；外层并发调用与同一文件缓存的同步需单独验证。destroy 必须等待在途查询结束，不能凭缓存锁推断释放安全。

头文件已有 gaia_set_worker_lease 和 gaia_set_cancel_checkpoint 注入；文件循环检查与调用者实际接线分别核对。无注入的直调路径可使用旧OpenMP默认team，不等于所有调用已遵守Runtime租约。模块不另建私有线程池，trace遵循唯一[开发开关](../../engineering/architecture/DEV_MODE.md)。

## 4. 完整性、所有权与错误

当前 MAX_FILES=32，单文件 collector 有 MAX_STARS_RESULT=200000 上限。超结果容量按已记录截断语义解释，不能把“查询成功”称所有恒星都已返回。目录装载失败、超文件数、记录损坏、解压失败、分配失败、取消和真实无结果分别报告。

接口结果数组由库分配，现有调用方按头/实现约定 free；跨DLL时需特别核对CRT和分配器，不把该legacy约定推广为任意模块ABI规则。client 的create/destroy成对，借用输入数组覆盖同步调用。GaiaCreateDiagnostics提供最近线程的装载计数/首个失败；快照在后续create时可能更新，调用方及时复制。

当前查询通常0成功、−1失败，空结果可成功；ABI错误码与进程退出码分别映射，不能因数值相同就套用CLI表。空目录的跨平台返回还需实际测量，上层必须检查真实装载数。错误时清空输出，不把残缺结果当完整。

## 5. 验证

独立解包微型XPSD并球面全枚举，检查赤道/极冠、RA回绕、星等量化边界、谱解码、坏块、分配失败、容量截断、冷/热缓存和合法释放顺序。剪枝局部推导和有限差分不证明所有锥零漏星；缓存字段一致不证明数据集输入完整。具体命令、原始字节、分母和失败记录随实验保存，本页没有新运行结论。旧参数表与实现自陈完整保留于[来源档案](../../../实验/documentation-refactor/docs/SPATIAL_NOTES_SOURCE.txt)。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/SPATIAL_NOTES_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Spatial source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
