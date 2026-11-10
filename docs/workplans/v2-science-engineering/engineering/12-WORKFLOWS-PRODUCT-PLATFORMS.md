# C2｜完整三命令工作流、双平台、GUI及可交付软件

## 总目标

最终必须有可以让用户真正从原始观测生成科学产品的、独立可执行的 Linux/Windows amd64 CPU 软件，而不是只能在 `validation/contract` 中调用的模块演示。核心流程是：
`FITS→Normalize（每帧）→持久化单帧产品→Mosaic（所有帧）→唯一signal HiPS→Export→二维WCS FITS`。
所有上层入口使用统一公开 Process/Host/Storage/科学计算实现；不允许CLI/GUI/driver各自复制一套WCS、Drizzle、Sky、Coadd或Export。

## W0：真实主线总装与显式数据边

1. `src/workflows/normalize.cpp` 严格按 [WORKFLOWS](../../engineering/rebuild/standards/WORKFLOWS.md) 组织校准、盲解/给定WCS、独立源测量、Gaia标度、Native应用、geometry/R、information与发布。不同帧复用同Gaia/source/master noise root必须是真共享；若外部相关依赖不完整，状态降conditional而不是伪独立。
2. `src/workflows/frames_provider.cpp`、`mosaic.cpp` 从完整manifest构造同一leaf order的合法帧集合，按响应/PSF可比较性做Sky control，先完成全局SkyFit屏障后创建只读`P后FramesProvider`，再调用 select/weights/integrate；不允许在迭代写最终tile时改β。
3. `src/workflows/export_workflow.cpp` 和 `src/projection` 必须只对已冻结的目标信号/支持做目标WCS的正确`Q`，同步解释response/covariance与partial，无原始FITS的全链product reopen路径也可运行。
4. 注册表、公开API、配置schema、应用参数必须互相可追溯，但不依赖“脚本检查了字段个数”证明真实业务已实现。实际载荷变更由本任务同步修正消费者、示例及真实输入。
5. 处理进度/中间过程诊断尽量沿现有有界probe体系，不为新主线建立第二个日志、cache/资源控制或科学算法脚本。

## W1：真实输入合同和本期功能边界

至少支持首期声明的：二维 ICRS TAN[-SIP] 可辨识、FITS原生科学输入、合法原始校准、给定WCS与可实际解出的无WCS输入、多个设备/像元尺度、相同目标NESTED order、Gaia参考/用户明确响应（含parametric窄带/实测曲线）、完整/部分/unknown噪声、单帧与多帧、同响应与显式声明的混响应、用户指定目标WCS导出。

不是本期承诺：所有FITS投影、所有畸变、完全未知PSF依然无偏恢复、自动校正任意大气DCR、无元数据自动定出真实gain、科学自适应超分辨(SACoadd)、GPU。超出支持域必须给精确、可理解的能力和原因，不能悄悄忽略PV/lookup、随意设QE=1或使用不合理fallback实现“能跑”。

## W2：CLI真实产品与错误语义

- `apps/cli` 需提供清晰的输入/输出、响应/星表路径、资源预算、科学容差/适用条件、日志目录和必要中间进度；不要求多余复杂GUI设置。所有参数均可定位到唯一公开Request字段。
- 终端/返回码要区分数据不合法、能力缺失、科学不可辨识、算法数值未认证、资源耗尽、被取消、磁盘/恢复错误。合法部分结果只有在科学协议允许的有效域内可被消费；不得将partial变成全图complete。
- 新进程分别执行三个命令，最终能用外部软件打开HiPS/FITS并核对源观测身份、BUNIT/WCS及合理科学量；单张FITS失败不应损坏先前完整产品。
- “输出文件存在/程序exit0”只证明软件跑完路径的一部分，不是算法精度/物理正确性的证据。
- 运行期间允许先small truth、再medium数据、再真正大型真实图像；每阶段需要明确可推导的被估计量并固定合法容差。

## W3：Windows/Linux构建与第三方I/O

1. 实际 Windows amd64 和 Linux amd64 完整构建**同一科学核心源码**，明确CPU后端、C++20编译器、依赖与许可证、安装包运行路径、动态库加载、locale与文件编码。任何只在Linux成功的断言不得推导Windows成功。
2. FITS写路径要验证 CFITSIO 现有实现或其它唯一合规实现；如果曾出现ffppx全零，需要独立最小API实验弄清何时写入/保存/关闭/类型/字节序，不得靠经验谴责成熟库并永久留两个并行writer。最终只维护一条生产写入路径。
3. 外部读回检查 header卡、END/padding/2880对齐、BITPIX、BSCALE/BZERO、BUNIT、NaN/−0/负signal、大小端、尺寸和实际目标WCS；使用成熟外库以及少量独立可手算例，不只同一个writer+reader互测。
4. 产品路径需支持常见Windows长路径/盘符/反斜杠和Linux文件系统，临时目录、读取权限、磁盘空间、进程被kill及原子改名的跨平台限制都要用实际环境核对；无法跨卷原子发布时应明确安全发布顺序。
5. 稀疏大数据/高精几何/外部catalogue的现实资源开销实际测量，不要假装大图是小toy。软件安装/启动不要求用户先安装开发工具或运行测试驱动。

## W4：GUI保持前后端分离

`gui/` 的Qt前端只负责配置、启动/停止、进度、日志/中间图、查看产品；科学计算是稳定的CLI或受控子进程/公开协议调用。GUI不能自己写星点定位、WCS匹配、PSF拟合和Drizzle fallback。必要交互包括：合法/非法输入提示、选择数据与响应、资源上限、状态显示、取消、失败原因、产品重开、科学条件/unknown的合理呈现。

做一次真实GUI操作：添加输入→参数核验→启动Normalize→查看中间状态→进入Mosaic/Export→查看外部可打开产物；另模拟用户取消、进程异常退出、配置冲突、过期科学模型/产品。不得因GUI存在目录或编译成功就宣布可使用。

## W5：科学真值端到端闭合

1. 从外部可信前向模型 `I_λ, f_s, PSF, PRF, WCS, QE, gain, sky, flat, master` 独立合成真实ADU和多帧跨根相关，不 import 生产`src/geometry/sky/coadd` helper。冻结独立真值后使用标准CLI三命令，不能让求解器看到真值中隐去的参数。
2. 依次做无噪声同响应、Poisson/read噪声同响应、不同PSF与WCS、共享标定误差、真实低频星云+差模、混合Hα/Red、偏星/坏点、极区/弱支持、动态几何精度、不同目标WCS Export。
3. 验收信号、单位/支持、星位、通量、背景差模、协方差/误差覆盖、混响应解释和最终FITS各层面；只对明确的 estimand 比较真值，不能将“单source Normalize测光”直接当“单leaf Mosaic场”的同一个物理量。
4. 用真实 HST/真实 M42/多夜多设备资料研究PSF/光学/大气/校准模型在真数据的适用范围；HST已经卷积/含噪时只能是带条件的模板真值。所有真实数据计算依照 [#103](https://github.com/fujiaze/Astro-CS-Database/issues/103) 的角色区分，不上传原始私有大FITS。
5. 重跑同一输入，在不同线程数/不同H/缓存开关/Windows/Linux比较，说明点值、误差证书与性能变动；任何科学模型或有效性不一致必须追踪，不因为“最后图片肉眼差不多”接受。

## W6：性能和资源是完成的一部分

**不要规定没有物理与目标依据的每帧固定运行秒数**，但必须得到现实吞吐：以输入像元数、NESTED order、PSF复杂度、Gaia星数、overlap nnz、期望科学误差、内存H/threads/CPU、磁盘类型记录Normalize/Mosaic/Export每阶段耗时、峰值RSS和I/O读写、fallback比例、未达标案例。区分“科学成功”与“预算耗尽”；不能用快速失败冒充加速。不允许无限高精几何迭代吃光所有资源，必须有显式科学/资源停止并给有效部分状态。

## W7：交付物

- Linux与Windows可独立运行程序、规范化真实参数示例及有物理意义的样例输入、小/中型可重现结果、产品目录约定和用户说明；
- 核心科学开发文档直接链接一手算法来源与本期真实源码，不用看旧版本才能理解当前；
- HST/M42实验清楚标明真实观测资料已得到/未得到，当前数据的条件性；
- 单一源调用路径的源码审查、完整三命令、新进程重开/故障恢复与外部读取证据；
- GUI实际操作和双平台真实运行记录；未获得硬件的平台/性能只能标未验证，不能从CI构建推断可用。

## 完成判定

一个不熟悉该仓库的工程师仅依据当前公开科学/工程文档和安装说明，能在支持的真实输入域执行三命令获得含有正确物理含义的产品，能按证据解释其数值/随机误差和未知项，并在失败时明白哪些结果有效。独立审核者从最终FITS反向追到原始观测、算法原文、同一实现代码与不可变噪声身份。
