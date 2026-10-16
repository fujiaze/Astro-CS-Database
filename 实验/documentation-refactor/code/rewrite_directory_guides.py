"""Rewrite reviewed directory entrypoints as linked reading paths."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
GUIDES = {
 'docs/detail/common/README.md': ('公共细节', '[统一对象引用](unified_objects.md)和[模型到接口](unified_model.md)连接科学正本、工程类型与消费者；[共享库](shared_libraries.md)说明公共实现和复用。'),
 'docs/detail/infrastructure/README.md': ('基础设施', '[Runtime](runtime.md)负责执行与寿命，[CLI](cli.md)连接配置和事件，[AIO](aio.md)与[存储形态](storage_form.md)负责读写，[可观测性](observability.md)与[日志系统](log_and_error_system.md)说明诊断。[星表](gaia_xpsd_client.md)、[机器画像](benchmark.md)、[浏览器](hips_browser.md)和[排障](troubleshooting.md)各按实际入口阅读。'),
 'docs/engineering/api/README.md': ('接口', '[公共 API](PUBLIC_API.md)规定接口分层、单位、所有权、错误和并发。跨库开发继续读[C ABI](abi/ABI.md)与[安全加载](abi/SECURE_LOADER.md)，具体符号由阶段模块正文和真实公开头定位。'),
 'docs/engineering/api/abi/README.md': ('跨库 ABI', '[C ABI](ABI.md)定义版本握手与稳定数据边界，[安全加载](SECURE_LOADER.md)定义路径、哈希、能力与模块校验。加载成功、执行和科学验证分别报告。'),
 'docs/engineering/build/README.md': ('构建与发行', '先按[构建图](BUILD_GRAPH.md)定位生产目标，再按[工具链](BUILD_NODES.md)选择实际 preset；版本、构建身份、安装材料和负责人发布责任见[发行合同](RELEASE.md)。文档均由 Git 跟踪；旧“本目录未入库”的过程描述已由当前文件状态取代。'),
 'docs/engineering/contracts/README.md': ('工程合同', '[Normalize 数据](NORMALIZE_DATA.md)规定逐帧字段与能力；[Runtime](RUNTIME.md)、[调度](SCHEDULER.md)、[命名块](PIPELINE_BLOCK.md)与[所有权](OWNERSHIP_LIFETIME.md)规定阶段内交接。[配置](CONFIG.md)、[CLI](CLI_PROTOCOL.md)、[日志错误](LOG_AND_ERROR.md)规定外部行为；[异步 I/O](ASYNC_IO.md)、[FITS](FITS_IO.md)、[HiPS](HIPS_IO.md)、[存储形态](HIPS_STORAGE_FORM.md)、[原子发布](ATOMIC_PUBLISH.md)和[清单](MANIFEST_VERIFY.md)规定磁盘产品。'),
 'docs/engineering/gui/README.md': ('Windows GUI', '[GUI 设计](GUI_DESIGN.md)说明 Qt6 独立构建、配置/分组、子进程 JSONL 调用和浏览。科学算法与产品目标引用[最高设计](../../ACSD_DESIGN.md)，不在 GUI 复制内核。'),
 'docs/engineering/resources/README.md': ('资源与测量', '[性能模型](PERFORMANCE_MODEL.md)说明预算模型与测量解释，[benchmark](BENCHMARK.md)说明机器画像；[CPU 后端](cpu/README.md)与[可观测性](observability/README.md)分别解释执行能力和观测。实际读数在实验中保存。'),
 'docs/engineering/resources/cpu/README.md': ('CPU 后端', '[后端架构](BACKEND.md)连接基线与动态组件，[ISA 变体](ISA_VARIANTS.md)说明逐内核选路与编译隔离，[能力探测](CAPABILITY_PROBE.md)和[AVX2 provider](AVX2_PROVIDER.md)给出具体实现入口。硬件可用、性能适合和真实调用分别核验。'),
 'docs/engineering/resources/observability/README.md': ('可观测性合同', '[结构化日志](STRUCTURED_LOGGING.md)定义日志行，[资源监控](RESOURCE_MONITORING.md)定义采样与字段，[运行图](RUN_GRAPH.md)定义计划/trace展示。三者通过真实运行身份关联，不能用计划值填观测。'),
 'docs/engineering/standards/README.md': ('工程标准', '[代码](CODE.md)、[注释](COMMENT.md)、[数值](NUMERIC.md)、[并发](CONCURRENCY.md)、[错误](ERROR_MODEL.md)、[缓存](CACHE.md)、[兼容](COMPATIBILITY.md)、[优化](OPTIMIZATION.md)、[依赖](DEPENDENCY.md)和[诊断探针](DIAGNOSTIC_PROBES.md)按主题约束实现。文档写法与追溯集中在[文档维护](../governance/DOCUMENT_GOVERNANCE.md)。'),
 'docs/engineering/testing/README.md': ('测试与证据', '[测试标准](TEST.md)规定独立真值、正负例和容差；[验证证据](VALIDATION_EVIDENCE.md)说明输入身份、执行、结果与适用域。编译、真实运行和独立科学验证分别报告。'),
 'docs/science/algorithms/common/README.md': ('公共数值方法', '[Gaia 查询](GAIA_QUERY.md)、[背景与参考精度](NOISE_ESTIMATION.md)、[诊断量与比较](GATES_AND_TOLERANCES.md)补充对应科学定义。接口与资源行为由工程及模块文档说明，比较目标不构成审批门。'),
 'docs/science/algorithms/export/README.md': ('球面与导出算法', '[HEALPix](HEALPIX_MAPPING.md)与[Drizzle 几何](DRIZZLE_GEOMETRY.md)补充球面坐标和交叠，[重采样](PHASE3_RESAMPLE.md)补充平面取样。目录内仍有[投影](PHASE3_PROJ_IMPL.md)、[重采样实现](PHASE3_RSMP_IMPL.md)、[FITS写出](PHASE3_FITS_IMPL.md)和[HiPS writer](HIPS_WRITER.md)的混合实现资料，当前调用以[Export 模块](../../../detail/export/modules.md)为入口；这些资料的全文拆分继续按原始清单记录。'),
 'docs/science/algorithms/mosaic/README.md': ('Mosaic 算法', '[背景求解](UPM_SOLVER.md)补充数值方法。覆盖、采样、UPM、排异、集成、写出和会话的长篇资料仍在本目录，实际入口见[Mosaic 模块](../../../detail/mosaic/modules.md)。统计定义引用噪声、背景和集成科学分册，不由实现清单另立产品模型。'),
 'docs/science/algorithms/normalize/README.md': ('Normalize 算法', '[校准](CALIBRATION_ALGORITHMS.md)、[缺陷处理](COSMETIC_ALGORITHMS.md)、[天测](PLATESOLVE.md)、[检测](STAR_DETECTION_ALGORITHMS.md)、[PSF](STAR_PSF_ALGORITHMS.md)和[测光](PHOTOMETRIC_FIT.md)补充专题数值方法。[Normalize 科学链](../../normalize/NORMALIZE_SCIENCE.md)规定依赖，具体接口从[模块正文](../../../detail/normalize/modules.md)进入。'),
 'docs/science/calibration/README.md': ('校准与缺陷', '[校准](CALIBRATION.md)定义 bias/dark/flat 约定、单位与误差；[CCD/CMOS 缺陷](CCD_DEFECT.md)定义检测、修复和能力状态。算法补充与实际接口分别从正文引用进入。'),
 'docs/science/detection/README.md': ('天测与科学源', '[天体测量](ASTROMETRY.md)定义 WCS、坐标原点、历元与独立残差；[星点检测](STAR_DETECTION.md)定义检测统计、召回与污染。IPV求解候选和WCS后science_sources职责分开。'),
 'docs/science/drizzle/README.md': ('球面 Drizzle', '[Drizzle](DRIZZLE.md)定义真实源面积、drop、交叠与最终归一算子，区分分配累加守恒、面亮度保持和孔径通量。协方差、覆盖与HiPS层级沿同一算子解释。'),
 'docs/science/formats/README.md': ('科学文件格式', '[HiPS tile 格式](HIPS_TILE_FORMAT.md)解释标准编码、层级、图像单位与项目支持域。格式库能力与三阶段实际发布通道分别说明。'),
 'docs/science/integration/README.md': ('排异与集成', '[排异](REJECTION.md)决定样本资格，[集成](INTEGRATION.md)定义接受样本的加权估计量、状态与误差解释。后选择方差与固定权重公式分别处理。'),
 'docs/science/noise_snr/README.md': ('噪声与参考精度', '[噪声分册](NOISE_SNR.md)定义背景/源噪声、PSF通量信息、条件参考SNR、空间重建和协方差传播。实际源、参考控制点、帧摘要与面亮度精度各有条件和单位，不能互换。'),
 'docs/science/normalize/README.md': ('Normalize 科学链', '[NORMALIZE_SCIENCE](NORMALIZE_SCIENCE.md)连接校准、IPV、WCS后科学源/PSF、测光、条件噪声与球面完成。[阶段架构](../../engineering/architecture/NORMALIZE_PIPELINE.md)说明生产交接，[数据合同](../../engineering/contracts/NORMALIZE_DATA.md)说明字段，[实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)区分目标与代码。'),
 'docs/science/photometry/README.md': ('测光', '[测光分册](PHOTOMETRY.md)定义真实响应、Gaia XP合成、逐帧标度与系统差异；[文献阅读](references/README.md)定位参考来源和待核验范围。拟合参数不确定度与像元随机噪声分开。'),
 'docs/science/projection/README.md': ('球面到平面投影', '[HiPS 导出](HIPS_TO_FITS.md)定义目标WCS、层级选择、反向映射、覆盖与适用域。实际投影/核支持见[Export管线](../../detail/export/pipeline.md)。'),
 'docs/science/psf/README.md': ('PSF', '[PSF分册](PSF.md)定义轮廓、全通量、像元积分与空间适用域；[数值方法](../algorithms/normalize/STAR_PSF_ALGORITHMS.md)解释拟合和残差尺度。[模块接口](../../detail/normalize/modules.md#star-psf)说明实际数组和状态域。'),
 'docs/science/resample/README.md': ('重采样', '[重采样分册](RESAMPLE.md)定义核、有效支持、重归一与协方差，区分采样点值和面积平均。[Export管线](../../detail/export/pipeline.md)说明现有调用，最终单信号不自动具备绝对误差。'),
 'docs/science/sky/README.md': ('相对背景', '[UPM](UPM.md)定义真实重叠控制、逐帧加性差模、连通分量规范、模型可辨识性与低频保真。接缝小不单独证明绝对天空或通带一致。'),
}

if __name__ == '__main__':
    for name, (title, body) in GUIDES.items():
        (ROOT / name).write_text(f'# {title}\n\n{body}\n', encoding='utf-8', newline='\n')
    print(f'Rewrote {len(GUIDES)} reviewed directory guides')
