# Normalize 球面参考精度独立复核

复核对象为 `950c261beb656d75113391cd85f77c79e8b5370c`，依据既有 Normalize 科学正本、NORMALIZE_DATA 和接口迁移规格。本次未修改生产 C++，未在计算节点重跑原始 FITS。真实帧读数来自该提交的 RUN.md/evidence.json；下面明确区分源码发现与本机独立执行。

结论：已有真实生产接线和持久化实现，但球面精度数学、算子归一、模型载荷与读取尚有实质错误，不能将本轮作为完整科学交付结案。既有合法 signal 不因这些参考精度问题被否定；本轮不切换 Mosaic 权重。

## 已核对的进展和证据边界

- 分支确实包含文档基线和报告中的七个提交，远端为所报 HEAD。
- 实际新增 noise 库 GLS、Drizzle overlap capture、sidecar serializer/reader 和 writer 发布调用。
- RUN.md 记录无原始 WCS 头的单帧实际经过 IPV；45 个内点和 0.0922 arcsec 是该次拟合记录，不是独立留出残差或全部 31 帧成功率。
- 该真实帧明确没有施加 photometry，不能据此验证 XP 标度后的 Poisson/参考通量链。
- 本机直接编译执行既有 sparse_reference_gls_test.cpp，全部通过；再直接编译同一生产源运行异方差反例，得到下面的失败。未运行整套 160 项测试，未独立重现真实帧或 ON/OFF 文件比较。

## F01：满列秩 GLS 的矩阵次序错误（已独立执行）

位置：`lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp` 的 m<=nl 路径及对应 public header。

令 Q=R V^(1/2)，G=R^T R，H=V^(1/2) G V^(1/2)，A_src=[P,Z]。正确的是：

`B = Q^T R A_src = V^(1/2) G A_src`

`M = B^T H^(-2) B`。

当前源码实现成 `B=G V^(1/2) A_src`。非均匀 V 与 G 一般不交换。R 满列秩时应严格得到 `M=A_src^T V^(-1) A_src`，可直接按该恒等式计算，并用 rank-revealing 分解验证前提；也可保留正确次序的等价路径，不能只交换注释。

反例：R=[[0.8,0.2],[0.2,0.8],[0,1]]，P=[0.6,0.4]，V=diag(1,9)，背景已知，不加源泊松项。R 满列秩且 V 正定。解析真值：

`V_F = 1/(0.6^2/1 + 0.4^2/9) = 2.6470588235294117`。

直接调用当前生产函数得到 `0.89692823838151814`，相对误差 `-0.66116044327809309`。原 dual 测试使用 V=I，恰好掩盖此错误。反例程序见同目录 `code/review_950c261.cpp`。

## F02：捕获系数是写盘前通量，不是最终 signal 的 R（源码确认）

位置：`normalize_precision.cpp::build_patch_windows`；对照 `drizzle_engine.cpp`、`astro_sphere_sink.cpp` 和 Normalize 科学正本 §8。

正式信号为 S_p=sum_j(w_pj x_j)/N_p，因此 R_pj=w_pj/N_p。当前窗口却保存 `op_c=w_pj*(D_p/N_p)`，这是 sink 的中间 flux 载荷系数，仍须除以 covered area D_p 才成为 signal。缺少这次除法使 surface_variance 和 transfer 的物理单位及数值错误。

最小量纲反例：一个像元 Ω=A_drop=a=2 sr，N=D=2 sr。正式 R=1/(2 sr)，当前 op_c=1；像元方差 v 映成正式 V_B=v/(4 sr^2)，不能是 v。

另一个独立问题：当前 D/N 在窗口内重新汇总，窗口边缘叶可能还接收窗口外像元，不能用局部归一替代正式叶的全贡献分母。应从同次生产累加器获取最终 N，并为所采用局部观测域完整捕获所需源贡献，或显式限制为完整支持的叶。不能因 overlap 面积同源就声称整个线性算子相同。

## F03：持久化模型标注与实际数值不一致（源码及本机载荷检查）

`sparse_reference.cpp` 实际设置 `lw.h_sphere=w.profile`、`lw.variance=v`，保存的分别是 m 维平面 P 和平面方差因子。接线层随后将前者标成 sphere 的 h、shape=(nl,m)，后者标成 sphere covariance、shape=(nl,nl)，单位却为 adu^2。并没有保存对应 shape 的 h 或 C_sphere。

本机反例 m=2,nl=3，返回 h 长度=2，variance 长度=2；球面 h 应长度3，显式 C 应有9项。常量背景 n_bg=1 时，`for(b=1;b<n_bg;...)` 不执行，背景数组为空。`covariance_rank` 又写入参数信息矩阵 rank：背景已知时是1，而本例球面 C 的秩是2。

遵守已有局部模型合同：保存实际 h=RP、C=RVR^T、Z=RZ_planar 和正确单位/维数/秩。若需要因子表示，须由前台先明确版本化 representation、domain 与 reader 求值合同，不能在显式矩阵标签下塞因子。PSF 模型还须保留源积分响应 P 与截断约定。

## F04：光度标度后的参考源 Poisson 项缺一个 alpha（源码确认）

`stage_frame_precision` 将 native F_ref 乘 alpha，但把原生 gain 原样传给 completion；后者加 `F'_ref P/g`。正确源方差是 `alpha^2 F_ref P/g = alpha F'_ref P/g`。可在原生面建模再变换，或以明确数组单位的有效 gain=g/alpha 求值，并保留原生 gain provenance。空间标度必须按逐像元 D_alpha 处理。

当前真实帧 alpha=1，无法暴露此问题。独立校验使用非1比例：确定性标度后 F'=alpha F、V'_F=alpha^2 V_F，条件 SNR 不变；同时验证读噪/背景不重复缩放。

## F05：reader 未满足正式读取合同（源码确认）

`normalize_science.cpp::read_frame_science_document` 把所有 model kind 设为 kPsf；未恢复 SIP 系数、control leaf/order、模型引用、accuracy 等关键字段。它不读取局部模型以校验维数/单位/h-C-Z-R 配对，也不核验模型文件 hash。发布函数另有 hash 比较，但不能代替独立 reader。

检测出 duplicate ID、错误 F/V/SNR、CD 奇异等 findings 后仍无条件 `r.ok=true`，发布方只检查 rd.ok，于是已识别的损坏数据仍能作为成功产品。应区别“可诊断地解析了 JSON”与“可作为 canonical 真源消费”；物理无效不得被标为后者。合法 precision unavailable 仍应正常读取。

必须完成无信息丢失 round-trip 和正式 §8 的引用、单位、索引、summary、hash 校验。损坏按数据错误处理，这是数据合同，不是研发审批门。

## F06：目标叶和支持域不能按最大覆盖量代替（源码确认）

`build_patch_windows` 用整个窗口 D_l 最大的叶作为控制点 target_leaf，而 RA/Dec 仍是 patch 中心。应由统一 HEALPix NESTED ang2pix 取中心叶，并验证该叶实际有效。叶号0也是合法编号，completion 的 `target_leaf==0` 不可作为缺失判据，应使用 optional/显式状态。

支持面积当前是小圆窗口内 sum min(D_l,Omega_leaf)，不是合同要求的非重叠 patch 支持分区；drop 面积之和截顶也一般不等于真实覆盖并集。恢复真实支持/孔洞、分区面积与局部估计窗口的区别，不能将1024像元求解预算当作整个 patch 的物理支持定义。

## F07：滤镜字符串记录未构成逐帧响应绑定（源码确认）

本轮确实移除了一个组级拒绝，但拟合请求仍使用 block 的 `filter_name`、`qe_name` 和 `doc.filter_passband`。新增 frame_filter_header 和计数只是记录，尚未按 input occurrence 解析真实逐帧响应/override；既有 request 下游仍有 declared-filter identity 校验路径。

须按原合同实现逐帧响应选择、真实响应指纹和可共享子组，缺模型如实标 unknown。不要仅增加几个标签就宣布跨滤镜标定完成。多设备/跨滤镜真实实验目前仍未运行。

## F08：typed Runtime 迁移仍未完成，不能解释成新架构待定

NORMALIZE_DATA §1 已明确要求扩展 BlockFrame/RunContext 承载 metadata/model handle；§4/§8 允许的是带 run/frame/revision/input-hash 身份的注册迁移 cache，不是永久靠目录猜取数。新增 f_prec_* 是 p1_op_drizzle 内的局部向量，不是 RunContext 帧对象槽。新 sidecar frame_id 仍来自 basename，input_occurrence 未在该生产赋值处绑定。

前台裁决：维持已定目标，不以现行 scheduler execute 方式撤销内存端口合同；逐边扩展 Runtime host 内部 typed payload/lifecycle，无需把 STL 放入公共 C ABI。大 HiPS tile 可以继续 AIO staging/spill，writer 继续唯一发布。恢复缓存与大面 staging 不等于用磁盘代替所有节点间对象。

`p1_session.cpp` 继续保持兼容 facade，当前不要求删除；没有新的科学理由要求退役。不要把本轮重点转成删除兼容文件。

## 下一轮实施依据和顺序

先完整重读上述正式正本及其实际源码，再按 F01/F02 修复数学与最终信号算子，F03/F05 修复可重算持久化与 reader，F04 完成非平凡标度传播；同步修复 F06 的叶/支持及状态表达。随后完成 F07 逐帧响应和 F08 Runtime 迁移。生产模型状态需明确全帧 PSF 近似、模板截断、未建母版/测光共同误差，不能只登记 master 后冒称其余完整。

保留现有真实帧结果及失败案例。修复后重算控制点、模型和摘要并标明新 SHA，旧 SNR=19.9989 不再作为已独立验证的精度依据。真实信号和原有 ON/OFF 记录保留其本来证据身份。未完成 sed_response、空间 PSF、母版误差和其它已披露缺项继续明确报告，不自行发明参考系统。范围仍限 Normalize。

## 本机复现

在仓库根执行：

```bash
g++ -std=c++17 -O2 -Ilib/include -Ilib/algorithms/noise_snr/include 实验/normalize-precision-p5/code/review_950c261.cpp lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp -o /tmp/acsd_gls_review
/tmp/acsd_gls_review
```

程序输出生产结果与解析真值；在所审提交上退出码1，表示反例成功暴露错误，不能改真值使它变绿。它仅编译对应生产算法单元，不代表全项目构建、真实数据或全部模式已测试。
