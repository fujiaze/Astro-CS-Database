# Alpha 版本合同与 Phase3 科学施工边界

## 1. 预发布版本

本轮只能产出 alpha，不得标 stable、release、RC 或 1.0。

- 版本格式：`MAJOR.MINOR.PATCH-alpha.N`，例如项目当前基础号为 0.9.0 时使用 `0.9.0-alpha.1`。Agent 必须先从当前唯一版本源读取 MAJOR/MINOR/PATCH，不得自行改大版本。
- 同一源码树只有一个权威版本源；CMake、CLI、manifest、schema、包名、run manifest 和文档通过生成/检查同步，禁止多处手填。
- 开发构建可显示 `X.Y.Z-alpha.N+g<commit12>.dirty`；正式 alpha 包必须来自 clean main，显示 `X.Y.Z-alpha.N+g<commit12>` 或按项目冻结的等价格式。
- `astrocs --version --json` 至少输出 version、prerelease=`alpha`、commit、dirty、build_id、abi_version、cli_schema_version。
- Alpha N 只能在最终外部审核通过后提升/打 tag；Agent 不得自行发布或创建 tag。

## 2. Phase3 冻结定义

输入是符合项目支持子集的图像 HiPS（HEALPix 层次球面 tile），输出是用户指定天区、平面投影、像元尺度、宽高和坐标系的二维 FITS image，带合法 FITS-WCS header、coverage/mask 和可追溯 metadata。

实现前必须引用并核对：

- Fernique et al. 2015, *Hierarchical progressive surveys*, A&A 578 A114（ADS bibcode `2015A&A...578A.114F`，arXiv `1505.02291`），以及 [IVOA HiPS 1.0](https://www.ivoa.net/documents/HiPS/20161122/PR-HiPS-1.0-20161122.pdf)；
- Górski et al. 2005, *HEALPix — a Framework for High Resolution Discretization...*, ApJ 622, 759（arXiv `astro-ph/0409513`）；
- Greisen & Calabretta 2002 [FITS-WCS Paper I](https://arxiv.org/abs/astro-ph/0207407)；Calabretta & Greisen 2002 [FITS-WCS Paper II](https://arxiv.org/abs/astro-ph/0207413)。

文档必须写出对应 section/equation/keyword，而非只列参考文献。至少核对 IVOA HiPS §3、§4.1、§4.2.1、§4.4.1、§6.3.1：HiPS 采用 HEALPix、目录映射、NESTED-only、tile pixel order/角尺度和客户端绘制流程；FITS-WCS 的 CRPIX/CRVAL/CD或PC+CDELT/CTYPE 语义必须回到 Paper I/II 原文。

## 3. Agent 不得自行决定的科学项

下列值必须在 `SCI-PHASE3-HIPS-TO-FITS.md` 作为显式合同写出，并由外部科学审核；缺项不得编码：

1. 支持的 HiPS 数据类型和 properties keys；tile order、tile width、NESTED 编号、frame；
2. 支持的输入坐标系和合法转换；
3. 输出 FITS-WCS 投影清单。Alpha 最小范围优先冻结为 TAN；增加 SIN/CAR 等必须独立测试，禁止“支持所有”空泛承诺；
4. 输出像素中心/边界约定、CRPIX 的 1-based FITS 语义、CD 或 PC+CDELT 规则、经度方向；
5. 根据输出 pixel angular footprint 选择 HiPS order 的公式和上下限；
6. tile seam、极点、经度 0/360、跨 tile 邻域的取样规则；
7. nearest/bilinear/area-overlap 等重采样器的定义及 alpha 默认；
8. 输入值是 surface brightness、flux-per-pixel 或未知时的行为；像素面积变化时是否/如何换算；禁止默认混淆 flux 与 surface brightness；
9. NaN、missing tile、coverage、mask、alpha channel、blank value；
10. variance/weight/support 的存在性与传播；未支持必须明确拒绝，不能静默丢弃；
11. FITS BITPIX、BSCALE/BZERO、单位、WCS keywords、history/provenance；
12. interpolation error、projection distortion、容差和适用 FOV。

若产品需求未给出某项，Agent只能在文档中列出 `UNRESOLVED-SCIENCE` 并继续其他独立任务；不得用个人偏好冻结。最终审核前必须清零。

## 4. 推荐但必须验证的 Alpha 最小实现

为避免无边界扩张，首个 alpha 的实现候选是：单通道图像 HiPS、明确 celestial frame、NESTED、输出 TAN FITS、用户显式 center/scale/width/height、nearest 与 bilinear 两种 sampler、coverage extension、float32/float64 输出。该列表是施工上限候选，不是未经审核的科学结论；SCI 文档可基于现有项目真实需求收窄，但不得悄悄扩大。

## 5. Phase3 独立 Oracle

必须至少有：

- 常数球面场：有效区输出恒定；
- 球面解析函数：输出像素中心经 WCS 反变换后的解析值；
- HEALPix tile 边界连续场：跨 tile 不产生人工接缝；
- RA 0/360、极区、旋转 CD matrix；
- 缺 tile/NaN/mask/coverage；
- surface-brightness 保持；若支持 flux 模式，验证球面/平面像素 solid angle 的换算；
- WCS round-trip pixel→world→pixel；
- baseline/ISA、1/N worker、Windows/Linux 数值合同；
- FITS header 用独立 WCS/FITS 读取器验证。

Oracle 不能调用生产 HiPS lookup/WCS wrapper/resampler。小规模允许高精度直接球面计算作为 reference。
