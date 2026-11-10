# B3｜真曲边球面 Drizzle 的可证明算法及高精度工程路线

## 目标：不是普通四角投影

从原生 `P_j` 在 detector 先按 pixfrac 收缩 `Q_j(f)`，连续 WCS/SIP 映球面 `D_j=W(Q_j)`；真目标 `H_p` 是 HEALPix NESTED leaf。真实定义：
`Ω_j=Ω(W(P_j)), A_j=Ω(D_j), a_{jp}=Ω(D_j∩H_p), w_{jp}=a_{jp}/A_j, F_p=Σ_jw_{jp}x_j, N_p=Σ_jw_{jp}Ω_j, R_{pj}=w_{jp}/N_p`，`B_p=F_p/N_p`。所有单位、支持、负号及误差预算明确。分母有公共 `A_j` 相关，不能当互相独立的区间中点误差。

## G0：从一手论文推到本项目新结果

原始 Fruchter–Hook 是线性 drop 分配；Calabretta–Roukema/HEALPix 赋予等面积 chart 与 NESTED 叶。**ACSD 自研候选**是面对连续曲线与精度预算的完整候选/包含型面积及高精度 fallback。重推 `dΩ=π/3 dsdt`、12face/`r=0,1,3,4` 分支、TAN/SIP曲线、Bernstein凸包与 `JΩW`；并证明主路线与 detector 域 `∫Q_j1_{H_p}(W)|J|du dv` 在相同科学量下等价。独立 B 不复用 A 的判定谓词。

## G1：严格候选与区域分类
- 先证明支持域 WCS 单射与允许的畸变模型，不能仅因当前输入是小天区就将曲边缩成四角。
- 目标域与 detector域都要有可实现的候选搜索；图幅面缝、极点 apex、LONPOLE、跨RA、切触、极薄正面积片与折叠未认证情况不漏叶。
- 区间 `outside/inside/undecided` 的每个判断必须附可检查的不变量：区域排除依据、连续边界包围、已证明inside质量、未决质量上界；预算结束仍含全部可能真实质量。禁止零采样命中→面积0。
- 数据结构保留精确/dyadic累计与原始 primitive；同一 `a_jp` 可为多消费者共享，不把对照算法的错误结果混入生产归约。

## G2：终值科学误差优先，而非统一面积百分比

从 `B=F/N` 对共同 `w=a/A`、`Ω` 的依赖推导有符号 numerator/denominator 的真正范围包围；对亮暗邻接、`±10¹²`、弱支持和任意request给出达到目标 `|ΔB|` 的充分面积预算。对 `C_R=RCRᵀ`、相关因子、sky/Γ/BLUE和Export请求分别推导有限扰动界，必要时反馈细化 geometry revision。若分母低界≤0但未全无支持，只能支持未决，不能猜一个中心或宣布 no_support。数值界是 deterministic，不是额外 random variance。

## G3：可运行的唯一 C++ 路线
- 阅读历史 `oracles/round2_affine.cpp`、`round2_mp_affine_fallback.py` 与已确认研究报告；分析正常数值路径和局部高精度策略的数学等价性与错误状态，不能把历史参考原型不加审查移植成正式 production。
- 正式目标：certified cut-cell/candidate → 局部可审查误差界 → 同一科学模型的高精度fallback；真正极点/大SIP/不可证明单射时走有明确资源上限的通用包含路径或诚实停止。
- 资源：逐块/逐源 bounded queue、共享边/导数缓存、稀疏 CSR 外存流、固定归约键、不能在大量源×叶上常驻稠密 overlap；workspace/metadata都进统一账本。单核/多线程/低H时不改变科学量或证书。
- 几何修订必须使依赖它的 R/PSF/noise/sky/选择/权/Export旧对象失效后重新推导，旧局部缓存只在依赖对象身份相符时复用。

## G4：必须再造的新反例
北/南极TAN及 face缝、`Nside=2²⁹`、tiny positive sliver/纯切触、无SIP和2–9阶、镜像CD、近奇异CD、不完整候选/外部cone漏选、f≠1的drop、原始Bayer/洞、负信号、FITS十进制卡导致的WCS变化、`work=1`、高对比科学误差超预算。每项解释**什么错误实现会错误通过普通面积/常量场测试**。

## 完成定义
- 两种真正独立的区域求积路线和至少一个解析真值，在明确输入域证明包含（不是百万随机点恰好没漏）；源码关键分支有数学理由。
- 确定性区间原语已可认证，R同一身份同时应用到signal、PSF、noise、background basis，图和科学误差均能恢复/细化。
- 大规模时间和峰值实际测量注明成功域、设备和误差目标；耗尽可以诚实未决，不用“跳过难像元”制造更快。
