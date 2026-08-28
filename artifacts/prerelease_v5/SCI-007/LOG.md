# SCI-007 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS SCI-007 行; 控制包 13_ALPHA §2/§3/§4/§5(12 科学项+文献清单+alpha 候选+Oracle 全集); DATA_SEMANTICS §2/§3(NESTED/leaf_order=tile_order+9)。

## 动作
1. 文献核验(web_search): Fernique 2015 A&A 578 A114 DOI 10.1051/0004-6361/201526075(全文页确认 §2 索引+目录结构、§3 HiPS↔MOC 主题); IVOA HiPS PR-HiPS-1.0-20161122 PDF 在线(13 指定节 §3/4.1/4.2.1/4.4.1/6.3.1 主题经检索印证,逐行标题复核留 ALG-P3-001 并显式声明); Górski 2005 文章级; Paper I/II 复用 SCI-002 已核验条目。
2. 新建 docs/science/PHASE3_HIPS_TO_FITS.md(SCI-P3-001): 15 节模板; §9a 十二项逐项冻结(alpha 显式拒绝清单/properties keys/仅 ICRS 恒等/仅 TAN/CRPIX+CD-only+parity east_left 默认/order_needed 公式=ceil(log2(sqrt(π/3)/(W·s_out_rad))) 上下限/跨 tile 双线性+RA wrap+极点拒/sampler nearest+bilinear 默认 bilinear/SB-only flux 显式拒/NaN-missing-coverage 语义/variance 输入显式拒/BITPIX-BUNIT-WCS-HISTORY provenance/容差与 FOV≤20°)。
3. §5 连续定义: order 选择公式+逐输出像素反向映射(TAN 逆→NESTED ang2pix→tile=ipix>>2log2(W))+coverage; §7 五不变量(WCS roundtrip 1e-6/常数场/bilinear 权重和=1/coverage 与 order 单调); §11 Oracle=13 §5 全集+独立性。

## 验证
- SCIENCE_CONTRACT_LINT_PASS files=1 sections=15。
- UNRESOLVED-SCIENCE=0(§15 断言, §9a 十二项全冻结)。
- 全量回归 unittest 19/19 OK。

## 产物
docs/science/PHASE3_HIPS_TO_FITS.md; 本日志。

## PASS 判定
严格执行 13 十二项; 原文定位到 section(限制级别显式); alpha 投影/采样/单位范围冻结且收窄不扩大; UNRESOLVED-SCIENCE=0; 不声称未实现的 variance/flux 模式(全部显式拒绝)。SCI-007 = PASS。
