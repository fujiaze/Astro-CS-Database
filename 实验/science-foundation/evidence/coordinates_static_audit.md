# S0 独立坐标核查与数值证据

核查基线：`e7d273ed78b7d118cf8344110b97224af7378b2a`。只增加研究脚本和证据，生产 C++ 未修改。下述数值为独立构造的合成真值，不是旧 issue 的读数，也不是正式科学验收。

## 阅读与依据

已读根 AGENTS、最高设计、科学索引、ASTROMETRY、PLATESOLVE、platesolve detail、星检测 detail 与 PSF 正本；沿实际 adapter 及生产翻译单元核对原点。标准依据：

- Greisen & Calabretta 2002, *Representations of world coordinates in FITS*, §2.1.1–2.1.2：[Paper I](https://arxiv.org/abs/astro-ph/0207407)。
- Calabretta & Greisen 2002, *Representations of celestial coordinates in FITS*, TAN：[Paper II](https://arxiv.org/abs/astro-ph/0207413)。
- Shupe 等，SIP Convention v1.0, §2, eqs. (1)–(6)：[NASA 正文](https://fits.gsfc.nasa.gov/registry/sip/SIP_distortion_v1_0.pdf)。
- Astropy `all_pix2world`/`all_world2pix` 明确 `origin=0/1`：[API](https://docs.astropy.org/en/stable/api/astropy.wcs.WCS.html)。

独立真值代码 [coordinates_independent.py](../oracles/coordinates_independent.py)，完整实测 [coordinates_independent.json](coordinates_independent.json)。复算：

```text
python 实验/science-foundation/oracles/coordinates_independent.py --production-probe
```

需要 Python、NumPy、Astropy；生产最小探针可用 g++，当前自动找到 MSYS2 GCC 16.1。环境实测 Python 3.12.2、NumPy 1.26.4、Astropy 7.1.0、WCSLIB 8.4。探针在临时目录编译原始生产源，结束后删除临时目录。`-flto` 只移除无关入口；不抽取或改写生产数学。星检测/PSF 探针的两种日志函数接空 sink，科学翻译单元原样编译，单线程运行；未执行全 Runtime 或本地 Gaia 目录查询。

## DERIVED：从标准重新推导

公共坐标 `p=(x,y)` 为 0 基整数像元中心，像元边界 `p±0.5`；FITS 连续像元坐标 `P=p+1`。`CRPIX=c` 为 FITS 1 基，因此 `u=P−c=p+1−c`；`u=0` 对应公共位置 `p=c−1`，与尺寸及是否居中无关。

SIP 与 TAN 前向：`z=u+D(u)`，`(ξ,η)=CD z`（degree），`n=normalize(n0+(πξ/180)east+(πη/180)north)`。手写向量 TAN 不复用 Astropy 的球面公式或任何旧 ACSD oracle。逆向解 `u+D(u)=CD⁻¹(ξ,η)` 后，公共输出必须 `p=u+c−1`。AP/BP 只可作近似初值，精度由前向残差和反演条件数控制。

连续矩形上若 `||∂D/∂u||∞≤L<1`，则积分沿连接两点的线段，得 `||D(u)−D(v)||∞≤L||u−v||∞`，由反三角不等式 `||F(u)−F(v)||∞≥(1−L)||u−v||∞`。因此 `F=I+D` 在域内单射，逆 Lipschitz 常数至多 `1/(1−L)`。脚本对每一多项式导数以系数绝对值与矩形最大幂给连续上界，不用随机采样冒充单射证明。当前构型 `L≤0.1104788293`，逆界 `1.124200338`；其数值计算是普通 FP64，未声称向外舍入的机器区间证书。

裁剪/分箱/翻转/转置统一以旧公共坐标 `p=Mq+t` 表示。令 `c0=CRPIX_old−1`，则 `CRPIX_new=M⁻¹(c0−t)+1`，`CD_new=CD_old M`，`D_new(z)=M⁻¹D_old(Mz)`。脚本对每行仅一个非零项的 M 做精确系数代换；这覆盖本次全部六类变换。分箱 `b_x×b_y` 且从旧首像元开始时 `t=((b_x−1)/2,(b_y−1)/2)`；此处只验证几何中心，不声称验证分箱的电子数或噪声变换。

## INDEPENDENTLY_TESTED：已执行的独立数值验证

构型有非整数 CRPIX=(47.381,39.127)、负行列式且非对角 CD、非对称 2/3/4 阶 SIP，含边界、参考点和随机点。三个切点分别位于 RA 回绕、北极附近及南极附近。每个切点/阶数 1003 点，共 9027 点。

| 检验 | 实际结果 | 解释 |
|---|---:|---|
| 手写向量 TAN/SIP 对 Astropy 前向 | 最大 3.626×10⁻¹⁰ arcsec | 本构型 FP64 精度，不是全仪器误差界 |
| Astropy 自身完整反演对真值 | 最大 5.249×10⁻¹⁰ pixel | 只表示该反演实现和此构型 |
| crop/bin2×3/flip-x/flip-y/transpose/组合 | 500 点/类，实测差为 0 | 两边进入 Astropy 前向的计算在此构型逐位相等 |
| 漏加 1 | 天球差 1.060–1.101 arcsec | 负例明确暴露 |
| 重复加 1 | 天球差 1.061–1.102 arcsec | 负例明确暴露 |
| 未转换 detector 半像元 | 天球差 0.530–0.551 arcsec | 坐标身份缺失可直接污染世界坐标 |
| 错原点正逆往返 | 最大 4.741×10⁻¹⁰ pixel，绝对误差仍约 1 arcsec | 往返通过不证明原点正确 |

生产 WCS 探针把相同 CRPIX/CD/SIP 与第三方天球真值直接输入原生产函数，共 1003 点：

| 原函数 | 实际结果 |
|---|---|
| `ipv::wcs_sky_to_pixel_iterative` | 1003/1003 收敛，但相对公共真值两轴均 +1 pixel；x 差 0.999999999771–1.000000001100，y 差 0.999999999903–1.000000000841 |
| `drizzle::WcsSip::pixelToSky` | 最大 3.626×10⁻¹⁰ arcsec |
| `pc::WcsTransform::skyToPixel` | 最大 3.732×10⁻¹⁰ pixel |
| IPV 对 CRVAL 严格对跖点求逆 | 错误返回 `converged=true` 且位置约 CRPIX；photometry 对同一输入正确返回 NaN |

原始引导检测/PSF API 的独立合成试验：64×64；整数中心真值 `(31.237,30.671)`；背景 100、振幅 1000、轴宽 3.2/2.6、角度 0.23 rad。Gaussian 与 Moffat β=4 分别按整数像元中心生成、各与自己的拟合模型匹配；这专门检验坐标原点，使用中心采样以排除像元积分模型误差。

| 原生产 API | 输入 prior | 输出中心 | 对整数中心真值的差 |
|---|---|---|---|
| `sdet_detect_guided_ex_f64` | 真值 | (31.73700000000002,31.17100000000002) | (+0.5,+0.5) 到 2.2×10⁻¹⁴ pixel |
| 同 API | 真值+0.5 | (31.73700000004467,31.17100000002490) | (+0.5,+0.5) 到 4.5×10⁻¹¹ pixel |
| `dpsf_fit_batch_f64` | 真值+0.5 | (31.23699999999951,30.67100000000147) | 最大绝对分量 1.5×10⁻¹² pixel |

Gaussian 的 +0.5 是其原有边缘坐标的正确输出；错误在于与公共整数中心坐标混用时缺少转换。DPSF 可通过拟合吸收初值偏移，不能因此认为 `StarSource` 中未改的 detector 坐标正确。

## 静态生产链与确切定位

1. `star_detection/src/sdet_api.cpp:795–796` 按 `x+0.5−cx,y+0.5−cy` 构造拟合样本；`:696–697` 返回拟合中心；`:1682–1683` 原样写出。引导入口 `:2178–2179` 也按 `round(pred−0.5)` 找索引。这一域是首像元中心 0.5 的探测器边缘坐标。
2. `platesolve/cpp/ipv/src/ipv_select.cpp:1979–1992` 将 detector 中心减去 `W/2,H/2` 并翻转 y。`ipv_wcs.cpp:290–291` 写 `CRPIX=(W+1)/2`；`:671–689` 将 CD/SIP 从 y-up 转回行号递增的 y 轴。故在公共整数中心 p 下，`det−W/2=(p+0.5)−W/2=p+1−(W+1)/2`，solver 核心原点自洽。不能据迭代逆 +1 就指称 IPV 导出的整个 WCS 普遍偏一像元。
3. `ipv_wcs.cpp:942–945` 在逆已求出的 SIP 相对偏移后输出 `u+CRPIX`，实际为 FITS 1 基；`include/ipv_wcs.h:42–74` 却将其定义成公共 0 基并禁止转换。独立生产探针验证两轴 +1。对 `wcs_sky_to_pixel_iterative` 全库搜索未找到实际消费者，不能声称这一接口错误已污染当前 CLI。
4. `ipv_wcs.cpp:814` 用 `abs(cosc)<1e−12`，未排除 q<0；对跖点被错误投到同一平面位置。photometry `wcs_transform.cpp:170–173` 用 `cosc<=1e−12` 正确拒绝。
5. `scheduler/src/module_adapters.cpp:3929–3947` 以 photometry 完整标准逆变换给出整数中心预测；`:4253–4257` 直接交给 detector；`:4283–4284` 又将 detector 原始 gx/gy 直接赋 `StarSource`。`:4292–4293` 对 gx/gy 直接 lround 查峰，未减 0.5。`:4361–4362` 原样交 DPSF，`:4421` 原样输出 source x/y。DPSF 的 `psf/src/dpsf_psf.cpp:519–520` 在整数中心采样，`:667–668` 输出 `cx+x0,cy+y0`；adapter `:4439–4441` 另存其 cx/cy，未回写 source。当前公开源表与 PSF 参数中心可能相差半像元；本报告已分别执行模块，完整 adapter 端到端尚未运行。
6. `drizzle/healpix_drizzle/wcs_sip.cpp:250–251` 正确使用 `x−(CRPIX−1)`；`:315–316` 逆桥接也正确。`drizzle_engine.cpp:1829–1830` 把整数中心像元的 `x±0.5,y±0.5` 当足迹边界。此模块不能接入未转换的 detector 域。
7. `platesolve/wrapper_phase1/wcs_tan.cpp:9–10,62–63` 使用 FITS 1 基输入/输出，单看 `x−CRPIX` 不能判 bug。实际 adapter `module_adapters.cpp:4985–4990,5494–5498` 先施加 `xp=x+1,yp=y+1` 再调 `p1_product_forward_sip`，`:4729–4738` 的 SIP 与 WcsTan 使用同一 FITS 域，桥接正确。
8. Export `projection/p3_wcs.cpp:519–520,562–563` 正确按公共输入 +1/FITS 逆输出 −1。`:463–469` 要求居中 CRPIX 是该导出生成器的局部能力限制，不能成为新通用 WCS 语义。
9. `noise_snr/cpp/src/snr_estimator.cpp:515–516` 使用整数中心到 FITS 桥接；其输入源坐标身份需要与 detector/PSF 一致。仅看到相同公式不构成已验证该调用链无半像元差。

## ASSUMPTION 与 REQUIRES_REAL_DATA

**ASSUMPTION**：新的参考坐标为有明确历元的 ICRS；TAN 使用切点所在半球；WCS/SIP 在有效探测器域连续、无折叠；轨道/大气像差可由此曝光的几何模型充分表达。有限 SIP 充分性属于模型假设，单射/数值逆正确不证明天测物理无偏。

**REQUIRES_REAL_DATA**：真实 HST/M42 的 SIP 拟合阶数与空间残差、Gaia 历元/自行传播、色差/大气差分折射、随时间变化的光学畸变、饱和/混叠导致的质心偏移、WCS 参数误差与星源残差的空间相关，以及 source/PSF/Drizzle/Export 整条真实观测链。这里的无噪声合成不是这些物理误差的证据。

本轮已解决数学原点定义及可复现反例；未改旧生产代码。独立审核者仍须从标准与已知真值自行复算，研究 Agent 自检通过不代表正式验收。
