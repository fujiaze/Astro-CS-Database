# 开发任务书与逐模块完成标准

从本篇分配实施任务。骨架在SYSTEM，冻结类型/签名在[FOUNDATION_API](FOUNDATION_API.md)，调度算法在[SCHEDULER](SCHEDULER.md)，计算函数在[COMPUTE_MODULES](COMPUTE_MODULES.md)，完整接入示例在[CALIBRATION_WALKTHROUGH](CALIBRATION_WALKTHROUGH.md)。这些是当前唯一目标合同，开发者不再自行决定另一套Task/资源/注册协议；内部容器、锁实现和文件拆分可在行为不变且验收通过时自行选择。

## 可以分工的实施顺序

| 里程碑 | 交付代码责任/具体结果 | 开始下一阶段的门槛 |
|---|---|---|
| M0 公共合同 | runtime公共头、科学Input wrapper/TypeId/codec、无重复聚合contracts.hpp；生成operation manifest | 所有目标声明C++20可编译；57项科学operation与文档逐项匹配，未知key/type负例拒绝 |
| M1 账本/所有权 | scope树、reserve/claim/close、allocator/Lease、work_reserved/consumed、不可变Block | S-LIFE/S-WORK与并发归因反例实跑，无双计/越额/UAF；未claim和存活输出寿命分别验证 |
| M2 Task/pool/pipe | 唯一CPU池、有限节点、suspend/resume、加权轮转、region R/F/E、原子分支credit/ordered join | S-FLOOR/FORK/ORDER/MULTI；P=1/多线程/取消/低H均完成或正确拒绝，等待不占CPU |
| M3 Storage/诊断 | async read/append/seal/abort、规范封存、raw/zstd当前格式、Host通用span和模块probe sink | S-IO、B01/H01/D01；短读短写/失败seal/磁盘满/异常/DEV_MODE OFF零求值；正式产品无半成品 |
| M4 首个科学接入 | 完整observation.calibrate，按实施例拆plan/kernel/adapter | C-CAL全部固定case通过；内存API/driver走同一registry，实际plan/usage证据齐全 |
| M5 模块并行接入 | 按下表依赖接入其余process与共享模块，不复制主算法 | 每模块typed边界+科学小例/负例+资源/取消+模型重开证据；未支持项明确返回unsupported |
| M6 工作流/发布 | Normalize/Mosaic/Export按WORKFLOWS显式组合，CLI/GUI协议统一 | C01—X01所需完整路径逐项实跑，双平台可完成baseline，干净安装/恢复/性能与故障报告 |

M0—M3可以在各自清晰接口下并行编码，但不得用mock的正常返回宣称M4生产通过；调用真实service的阶段性集成是门槛。GPU代码不属于M0—M6，只需CPU能力报告/显式gpu请求失败与未来事件合同模拟。旧接口/旧产品不作为验收基线，Git历史不改写。

## 模块plan和工作单元表

以下n是本次cohort内输出/输入行数，K是参与帧数，p是局部/全局参数数，v是几何顶点数，b是精确位长，s是一次触及的源数。`W_module(args)`是该模块CPU实现对**实际分配布局**给出的字节上界函数，必须在模块plan文件实现并用allocator分配日志验证；不是留给使用者填写的配置或一个可忽略估计。嵌入库不提供界时先使用已验证稠密/小问题分支，超预算返回resource_exhausted，不臆测隐藏库内存。

| process | 固定工作key/可拆轴 | R、单cohort必要项及barrier | 实现依赖和首要独立case |
|---|---|---|---|
| observation.calibrate | (epoch,flatten_begin)，min n=1 | R=四数组metadata+noise/error/control；F=34n输入+26n输出+W_interval/codec | arrays/numerics/uncertainty；C-CAL |
| observation.regroup | 输出G行段，min n=1 | touched parent、CSR nnz、footprint、输出/界/DQ；成员并集不可拆成漏孔 | calibration/operators；U01 |
| coordinates.wcs | 点批/Box key；min一个点或Box | R=WCS/证书；F=点/Box/多项式阶/逆根盒+W_coordinates(b)；整域准入屏障 | numerics；C01 |
| catalogue.access | sky shard/page/source_id范围 | R=离线index元数据；F=页/XP/quad排序缓冲；建索引外存归并 | coordinates/uncertainty/Storage；目录历元/损坏页负例 |
| sources.astrometry | 检测块/候选序号 | R=固定候选/已验证最优；F=窗口/匹配+W_solver(p)；候选接受按ID屏障，不按完成先后 | catalogue/WCS/numerics；C01/X01 |
| sources.psf | 独立混叠窗口组/模板行×源块 | R=PSF/参数盒；F=窗口样本×源、Jacobian/积分+W_psf(p,b)；同组联合 | WCS/numerics/operators；F01 |
| sources.measure | 独立窗口组，最小完整组 | R=源表/joint_noise/error；F=真实模板/背景设计矩阵+W_measure；不截断邻源 | PSF/uncertainty；F01 |
| response.integrate | (response,basis,segment) | R=预测维数d；F=分段面板/节点/bits+W_integrate；固定分段归约 | numerics；解析多项式/尾界 |
| photometry.fit | source局部块，min一星；全局参数region | R=θ/共享Schur/根图；F=每星latent/谱与W_photo(p)；全局联合解屏障 | measure/response/numerics；F01 |
| photometry.apply | parent原生行段，min n=1 | R=新模型/α链；F=输入x/参数区间/输出、G依赖+W_scale；不是bin中心α | photometry/operators；梯度α/重复应用 |
| geometry.overlap | source/Box固定细分key，min一有界primitive | R=Ω/A共同ledger/primitive；F=候选/v/b/排序+W_geometry；自适应重算F后扩展 | WCS/numerics；G01 |
| operators.action | apply目标行/transpose源分区 | R=recipe/index；F=touched输入+CSR(nnz)+输出/界+W_reduce/spool | numerics/Storage；U01转置恒等式 |
| uncertainty.covariance | 触及根/因子分区 | R=共享根图；F=稀疏因子/touched泛函+W_cov；不物化N² | operators/numerics；U01同根交叉项 |
| geometry.drizzle | 固定target行、source归并key | R=R/support/共享依赖；F=spool归并nnz/x区间/输出+W_drizzle；同R所有量 | overlap/operators/uncertainty；G01/U01 |
| information.reference | 位置/模板列，min一查询 | R=参考模型/误差；F=h/Z/触及C因子+W_information(p)；可估空间完整 | PSF/response/C；F01 |
| information.scene | leaf查询或chart三角网 | R=控点/网/holes；F=halo/精确谓词/查询+W_triangulation；整三角支持判定 | covariance/geometry；V01 |
| sky.controls | 同order patch/组 | R=连通图/控制模型；F=公共支持/T/H/A/E与控制C；分组完成再解 | information/operators；S01 |
| sky.fit | 真实连通分量；不可任意拆同分量 | R=β/K/P/nullspace；F=联合系统/白化/KKT+W_sky；全分量冻结屏障 | controls/numerics/C；S01 |
| coadd.select | leaf块，min一leaf的完整K | R=Selection/mask；F=样本/DQ/实际Γ/LOO+W_select(K) | frozen sky provider；W01 |
| coadd.weights | leaf/相同mask批次，完整K | R=WeightSolution/error；F=K² Γ/PSD/active set+W_weights(K)；无独立IVAR偷降级 | selection/C/numerics；W01 |
| coadd.integrate | target leaf块，min一完整叶 | R=最终模型；F=已选样本/权/共享noise/归约输出；只消费显式Selection/WeightSolution | weights/sky/operators；U01/W01 |
| projection.export | 目标WCS块，min一目标像元 | R=Q/闭包/输出metadata；F=相交真实叶/支持/halo/输出+W_export；只到ExportResult | geometry/operators；E01 |

分块大小先取process文档/上表允许值，按SCHEDULER减半到最小值；最小单位是完整科学相关组时不能减到任意一个像元。`W_module`要计纯库函数的临时/大整数及排序/压缩，不把线程数当内存上限。数学上存在稀疏法不代表当前实现已有：CPU分支能力必须登记，无可行路线就具名拒绝。

## 完整模块包与自动登记产物

每个模块只维护一份process descriptor源、一组公共typed请求/输出与codec声明、相应文档和实现。构建扫描显式模块清单（不扫描任意磁盘插件）生成：

1. builtin_processes.cpp：按process_id稳定排序调用各登记函数，不写算法分派switch。
2. process-manifest.json：process/operation/TypeId、依赖、CPU能力、probe owner、文档和semantic_revision。
3. 输入/输出schema与codec绑定表：从相同typed声明导出；wire没有指针、arena或writer。
4. 接口/文档覆盖检查：22个process及57个operation与声明一致，唯一probe owner，无已声明但缺run/plan/finalize的半模块。

以上文件是后续构建产物，本轮不增加当前可执行schema或假脚手架命令。“一键接入”的验收是：复制模板包填本地typed绑定和CPU实现、增加显式构建源清单后，自动生成上述四项，Host核心不用改；省略任一codec/文档/例子应构建失败。

## 每次开发交付必须给出的证据

冻结输入/容差/hash在被测运行前完成。提交至少含合法精确例、必要科学负例、单位/index/缺能力、低H/尾块/取消/短读短写、单/多线程与DEV_MODE状态。观察文件由真实registry和service产出，不能用手写expected当observed。资源报告同时给plan R/F/E、held/allocated峰值、物理I/O/累计spill/work和原始Issue。

模块完成后独立调用、组合调用、CLI/driver必须调用同一实现；full模型新进程重开保共享根/误差/来源，signal_only缺能力具名。性能先冻结可完成baseline再比较中位/p95与端到端时间，失败快不算优化，kernel-only不算总加速。没有实际GPU实现不报告GPU结果。

实现者可以改变私有容器或优化内存布局，但公开行为、科学规则、资源额度、固定key/归约、partial/取消/错误、所有权和产物身份不得自行改变。若这些需要改变，先给反例修本主题正本并联动消费者，不能在某个process临时造绕过框架的通道。
