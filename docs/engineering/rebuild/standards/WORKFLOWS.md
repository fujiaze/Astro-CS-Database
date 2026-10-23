# 固定命令的process编排

本篇只拥有科学数据边和阶段屏障，调度器/生命周期只由[SYSTEM](../SYSTEM.md)拥有。工作流不含求解或像元算法。边由输出类型/单位/index/content hash及科学能力验证；process descriptor的depends_on表示静态代码/能力依赖，不自动推断一次运行的数据图。一个process可出现多个invocation，每次都给显式输入和唯一invocation_id。

## Normalize

1. 读取观测、校准输入、响应与时间上下文；建立源身份、单位和校准随机根。校准/修复输出新观测版本，显式保留 G 或非线性条件。
2. 校准像元 → solver 候选 → 本地 Gaia 匹配 → 完整 WCS；输入合法 WCS 是具名的另一来源。无头 WCS 必须实际求解；unsolved 帧进入运行清单，不能发布伪球面帧。
3. 完整 WCS + 校准观测 → 独立 science_sources → 像元积分 PSF 与 counts 测量。Gaia 历元传播、DCR 假设及源选择随结果保留。
4. 实际响应 + XP 表示 → 分段积分；counts + XP 共享 latent → 联合 photometry；对原生每像元应用 α(u) 一次。application_id 与输入/输出单位验证重复应用；空间 α 同样作用于模板，不用星心标量替代全 PSF。
5. 校准/标度后的观测、实际场景噪声根和拟合影响构成统一 NativeObservation。纯初始噪声统计、响应基积分及已知 WCS 的几何准备可并行；最终噪声模型必须绑定实际应用版本。
6. footprint + WCS + 网格 + science budget → Ω/A/a → R；R 同时变换 signal、PSF、噪声根和背景基。先请求粗界、按 B/F/Cov 等目标反馈细化，直至达标或资源耗尽。feedback 是同一次操作的受控求解，不是下游改写上游对象。
7. information 按同一次 R 计算参考 V_F、actual-scene V_B 及需要的 controls，各自带 condition。writer 发布单帧产品闭包；噪声未知仍可有合法 signal，数值未达标不能当认证结果。

跨帧共享 Gaia 星、母版和响应参数不被帧边界截断。首期支持多帧 photometry 的按 source 分块联合求解；独立 normalize 调用也必须保留相同外部 latent 身份、源测量依赖和影响 action，供后续恢复交叉 covariance。若联合输入依赖不完整，降为具名 conditional/unknown，不能只存每帧参数协方差后声称相互独立。

## Mosaic

1. 从完整发布 manifest 打开用户选定产品；统一 ICRS/NESTED、同 leaf order、可解释单位。混 order 明确拒绝；不同仪器/滤镜名不是拒绝条件。缺失参考 SED 转换时要求显式混合目标，不伪造相同物理量。
2. provider 暴露逐帧 signal、邻域、support、PSF/response、noise 和原生到叶 action。重复产品/source 身份核对，重复读取不能制造新观测。actual-scene 方差优先直接 action；仅有稀疏控点时按其条件、有限支持及外部曲率界重建。
3. 对响应和有效空间观测可比较的帧构造控制 T、共同天空 H 与 A；需要 PSF 匹配时只使用可实现的共同较宽核。不同响应不建伪差模边；无可比控制的帧保留零修正规范及“未测相对背景”状态。
4. 逐真实连通分量拟合相对 sky，保存全部共同常量/斜坡及其它不可辨识模式、gauge、秩、K/T/E。控制足迹外的修正带预测状态。背景拟合形成全局屏障，不能边写最终 tile 边改变 β。
5. 应用 P=I−EKT；逐目标块确定资格和可选排异，再按 policy 求权及归约。Γ 必须含 shared_id 和 sky 同数据项。需要几何依赖的 K/权/选择精度时，反馈至可持久恢复的 primitive；不可恢复则报未决，不冻结中心解冒充无条件界。
6. 写入唯一 signal HiPS。有效核、支持/选择、规范、数值界与可选 covariance 模型作为模型元数据/辅助块保存，不发布第二个 variance/weight/sky 科学图像通道。

共同均值且 Γ 完整才称 BLUE；要求非负响应时用非负 QP。异 PSF/通带按实际 t 与 c 保存 K_eff 的空间、光谱耦合，不能总分离为一条有效滤镜曲线。unknown noise 默认具名 equal且不标 full_IVAR；首期无自定义经验权或启发式排异入口。小 N 默认无时间排异；启用单步 LOO 基线须 N≥4、共同可比较估计量/均值和外部固定可信 Γ，只拒一个最大 LOO，不并行拒所有超阈样本。异 PSF/响应的真实均值差、时变源不能直接送入共同均值 LOO；无足够可比性证据时跳过统计排异，保留合法混合 signal。由同数据估计的 Γ 或非Gaussian场景不满足首期single_max_loo前置时必须skip，保留条件估计/原因，不附清洁Gaussian基线的family误拒保证。启发式排异只属未来科学扩展讨论，当前不可请求。未来 SACoadd 可替换合成器，但本阶段不设计其数学算法。

## Export

目标 WCS 使用同一 coordinates/provider 模型域。支持均值 Q_ip=b_ip/Σb 与支持积分 Q_ip=b_ip 是两个独立请求，默认输出支持面亮度均值；整像元通量外推需显式选择。b=Ω(E_i∩H_p∩U_p)，不做第二次 drop 收缩。只有 full-leaf 近似支持时标明近似，不用一个比例恢复子叶形状。Q 对 signal、可用 noise 和 response 同步作用；只有最终 signal 时 noise=unknown。无支持为 NaN，负值与零保留。HiPS 层级派生也保存自身算子/支持近似，不能把低阶层伪装成重新 Normalize 的同阶独立观测。

## 绑定表与重算

| 工作流步骤 | 注册process |
|---|---|
| 输入/校准/可选G | 基建reader → observation.calibrate → observation.regroup |
| 已知WCS或盲解/科学源 | coordinates.wcs或sources.astrometry；catalogue.access提供目录；sources.psf → sources.measure |
| 响应与联合标度 | response.integrate → photometry.fit → photometry.apply |
| 交叠与同R | geometry.overlap → geometry.drizzle；operators.action及uncertainty.covariance供公共运算 |
| 信息查询 | information.reference / information.scene |
| 相对背景 | sky.controls → sky.fit，全分量冻结屏障 |
| 资格/求权/合成 | coadd.select → coadd.weights → coadd.integrate；integrate显式消费前两者输出，不重算或隐藏会话 |
| 目标导出 | projection.export → 基建FITS/产品writer |

GeometryRevision反馈由工作流控制器重新提交受影响invocation；总work不重置。重算只走已登记process和明确新版本数据边，不让下游原位修改上游对象，不把算法迭代实现成无限递归submit。嵌套helper不等待同一耗尽线程池；框架并行原语采用协作执行或串行内联。

产品publish/open和原始FITS reader是Storage服务，normalize/mosaic/export_image是固定组合入口；它们不假装一个新的科学算法节点。当前类型构造器、provider方法和纯数值helper也无需每调用一次就生成调度节点，但对外可执行科学operation全部必须有所属process和typed binding。具体映射由模块登记构建检查。


### Normalize固定展开

Normalize调用顺序确定为calibrate→可选物理regroup→solve/supplied WCS→bind_wcs→fit_psf/measure→bind_observation_model→integrate_response/fit_photometry→apply_scale→geometry/build_drizzle→information→publish。一般linear_view不是严格Drizzle输入，Normalize回原生parent投影须在运行配置中显式记录此路径；不得无提示以Gx代替原生。

### 合成的显式中间数据边

sky.fit冻结后，工作流通过共享provider构造服务将apply_sky产生的P后signal/noise/response装配成只读FramesProvider；不复制科学算法，不另拟合背景。该provider及SkyFit hash显式绑定，然后依次invoke coadd.select、coadd.weights、coadd.integrate各一次。最后一个typed输入明确包含Selection和WeightSolution，禁止integrate内部再算选择/权。中间对象可以只驻内存或spill，普通finalize不强迫每一步产品落盘。
