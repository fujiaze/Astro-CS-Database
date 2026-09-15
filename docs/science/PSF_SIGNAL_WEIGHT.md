# PSF Signal Weight 科学定义与可选集成模式

文档 ID：SCI-PSFW-001
状态：TARGET_NORMATIVE
上位：docs/science/UNIFIED_SCIENCE_MODEL.md
依据：PixInsight PSF Signal Weight/PSF SNR 方法、Horne/Naylor 最优提取、Zackay & Ofek 多图像点源最优组合。

## 1. 项目决定

AstroCS 正式支持 PSF Signal Weight 类方法，而不是把它仅作为屏幕上的 QA 数字。但“PSF Signal Weight”必须分为两个不会混名的产品：

1. psf_information_weight：基于观测模型的点源信息权重，默认科学模式；
2. psfsw_robust_weight：受 PixInsight PSFSW 启发的稳健复合帧权重，可选工程集成模式。

二者都可以参与 Phase2，但目的、单位、归一化、最优性声明和输出字段不同。

## 2. 严格点源信息权重

对已归一到公共通量尺度的帧：

~~~text
Q_k = a_k P_kᵀ C_k⁻¹ d_k
W_info,k = a_k² P_kᵀ C_k⁻¹ P_k
F_hat = ΣQ_k / ΣW_info,k
Var(F_hat) = 1 / ΣW_info,k
~~~

这是 science_objective=point_source_detection 或 point_source_photometry 的默认权重。白噪声近似为：

~~~text
W_info,k = a_k² / (sigma_pix,k² A_NEA,k)
A_NEA,k = 1 / ΣP_k,p²
~~~

它同时反映透明度/光度尺度、背景与读噪、PSF 集中度，并具有 1/flux² 信息单位；在模型成立时可宣称最小方差/最大点源 SNR。

## 3. PixInsight-style 稳健 PSFSW

PixInsight 将 PSFSW 定义为 hybrid PSF/aperture photometry 的综合图像质量估计器：PSF 总 flux 表示总 signal，mean PSF flux 表示 signal concentration，分母结合稳健 noise 与稳健 mean background；归一常数把典型数值调到实用范围。其 PSF SNR 则采用 ratio-of-powers，并被建议用于只追求集成图像 SNR 的权重。

AstroCS 实现 psfsw_robust_weight 时必须保留这些特征，但为避免星表选择偏差增加以下约束：

- 只在同一波段、同一目标/重叠连通分量、光度已归一的帧组内比较；
- 使用跨帧匹配的共同恒星集合或显式 selection-function 校正；
- 排除饱和、混合、拖线、移动源、严重 PSF 失配和边缘截断源；
- signal、concentration、noise、background 四个分量分别写入产品；
- 指数、截断、稳健估计器和归一常数版本化，训练/调参样本不得与最终验收样本相同；
- 输出无量纲相对值，按组归一（如 median=1），不得写入 ivar、variance 或 W_info；
- 无共同星集、背景非正且变换未定义、有效星不足或选择偏差门失败时 unavailable，不回退成 median source SNR。

项目不要求逐字复制 PixInsight 的实现常数；项目公式必须通过公开文献语义、独立推导和真实数据优化后冻结。若选择精确兼容模式，则字段另命名 pixinsight_psfsw_compat 并记录所兼容版本。

## 4. Phase2 支持的选择

| mode | 权重 | 用途 | 可作最优性声明 |
|---|---|---|---|
| point_information（默认） | W_info 或 Q/W | 点源检测、点源测光、proper coadd | 模型和 covariance 门通过时可以 |
| psfsw_robust | psfsw_robust_weight | conventional image integration，兼顾 signal、星像集中度、噪声、背景 | 只能声明在验收数据上优于指定基线 |
| psf_snr_power | ratio-of-powers 的项目冻结实现 | 只追求集成图像经验 SNR | 不能自动等同 Fisher 最优 |
| surface_gls | AᵀC⁻¹A | 扩展源/面亮度 | GLS 假设成立时可以 |

Phase2 配置必须显式选择 mode。默认不得由检测到多少颗星等偶然因素自动切换。

## 5. 复合权重与不确定度的边界

psfsw_robust_weight 可以决定 conventional coadd 中帧的相对贡献，但不能反向定义输出像素 variance。最终 variance/covariance 必须从实际线性组合系数和输入 covariance 传播：

~~~text
C_out = R C_in Rᵀ
~~~

若复合权重还含 seeing/concentration penalty，它会改变分辨率与噪声折衷；必须输出 effective PSF，并报告相对 point_information 和普通 ivar 基线的 detection power、FWHM、通量偏差和面亮度偏差。

## 6. 标量与空间模型

- W_info(x,y) 默认是空间量；只有通过均匀性/信息损失门才压为帧标量。
- psfsw_robust_weight 是帧组内相对标量，但其四个输入分量必须带空间摘要 p05/p50/p95 和有效覆盖；显著空间非均匀时拆成 region/tile 权重或拒绝标量模式。

## 7. 强制验收

1. 注入点源验证 1/sqrt(W_info) 与实测 flux dispersion；
2. 透明度、seeing、背景和 read noise 单变量扫描方向正确；
3. 改变不相关星表深度/检测阈值不得显著改变共同星集 PSFSW；
4. psfsw_robust 在预注册 M42/银心及合成集上，与等权、exposure、pixel-ivar、W_info 比较；
5. 分别报告点源 detection power、photometric variance、effective PSF、扩展源偏差和伪影；不得只以“看起来更好”通过；
6. 零星、少星、拥挤、严重梯度、云、拖线、不同 FOV 和不同波段都有 fail-closed 测试；
7. 权重分量和最终权重的负向 mutation 必须使门变红。

## 8. 禁止事项

- 把实际恒星 median(SNR) 直接命名 PSFSW；
- 把 PSF 拟合 residual、FWHM 或背景任一项单独冒充完整 PSF signal weight；
- 把无量纲复合质量写入 inverse variance；
- 未声明共同星集/selection function 就跨天区比较 PSFSW；
- 用 PSFSW 的经验成功替代 Q/W 与 covariance 的科学产品。
