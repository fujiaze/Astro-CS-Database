# Phase3 HiPS→FITS Resample Algorithms (ALG-P3)

> ID: ALG-P3-001  范围: ALG-P3-001..004  上游 SCI: SCI-P3-001  状态: DERIVED (V5 ALG-007, 2026-08-28)  模块: phase3 (待实现, 本文档为施工规格)

## 1 上游 SCI 与输入输出

- 上游: `SCI-P3-001`（PHASE3_HIPS_TO_FITS.md §5 连续定义 + §9a 十二项冻结）
- 输入: HiPS 目录(properties+tiles, float FITS) + 用户参数 center(RA,Dec)/s_out/W_out/H_out/sampler(nearest|bilinear)/parity(east_left|east_right)/bitpix(-32|-64)
- 输出: 单张 FITS image(S+C=coverage, 经掩膜合成或独立 COV 扩展由 API-004 冻结) + provenance(HISTORY); 落盘原子(tmp+rename)
- 前置依赖: `lib/common/healpix`（ALG-HEALPIX-*，round-trip ≤1e-12 deg, NESTED 父子一致）

## 2 离散公式

```text
G1 (ALG-P3-002) 输出 WCS 构造 (FITS 1-based, CD-only):
  CRPIX1=(W_out+1)/2, CRPIX2=(H_out+1)/2, CRVAL=center
  |CD1_1|=|CD2_2|=s_out, CD1_2=CD2_1=0
  east_left:  CD1_1=−s_out, CD2_2=+s_out
  east_right: CD1_1=+s_out, CD2_2=−s_out

G2 (ALG-P3-002) 反向映射 (逐输出像素 (x,y), 1-based→中间平面):
  iwc = CD^{-1} · ((x+1)−CRPIX1, (y+1)−CRPIX2)      # deg 偏移
  world = TAN^{-1}(iwc; CRVAL) → (RA,Dec)∈[0,360)×[−90,90]
  gnomonic: (ξ,η)=atan2 形式; 球面角差按 RA wrap 归一

G3 (ALG-P3-003) order 选择 (SCI-P3 §5 冻结):
  s_tile_rad(order) = sqrt(π/3) / (2^order · W)     # tile 像素角尺度近似
  order_needed = ceil( log2( sqrt(π/3) / (W · s_out_rad) ) )
  order_sel = clamp(order_needed, 0, hips_order)

G4 (ALG-P3-003) leaf 采样:
  leaf_order = order_sel + tile_shift(=9, W=512)
  ipix = ang2pix_NESTED(nside=2^leaf_order, RA, Dec)
  tile = ipix >> (2·log2(W));  local = ipix & ((1<<2·log2(W))−1); (lx,ly)=nested_local_to_xy(local)
  nearest: S = tile[lx,ly]（有限判定→coverage）
  bilinear: 邻域 4 leaf 权重 w=面积重叠分数(投影线性化), Σw=1, S=Σ w·tile_value, 跨 tile 读相邻 tile

G5 (ALG-P3-004) FITS 写:
  BITPIX=−32/−64, BSCALE=1, BZERO=0, BUNIT=properties(缺省 'ADU')
  WCS: G1 全量 + CTYPE=TAN + CUNIT=deg; HISTORY: 源 HiPS 标识/order_sel/sampler/软件版本/manifest hash
  coverage: C=1 ⇔ 足迹内存在有限 tile 像素; 无覆盖 S=NaN
```

推导来源: **SCI-P3-001 §5 连续定义与 §9a 冻结回答的离散化**（G1↔§9a-4, G2↔§5 反向映射, G3↔§9a-5, G4↔§9a-6/7, G5↔§9a-11）；实现一致性锚（非推导依据）: 待建 `lib/phase3`。

## 3 伪代码

```text
function phase3_resample(hips_dir, params):
  props = read_properties(hips_dir)                    # ALG-P3-001: 必需键校验, 非法显式拒
  validate(params): frame=icrs, W,H∈[1,20000], s_out>0, |center.Dec|≥5°, pixfrac N/A
  order_sel = G3(props.hips_order, W=props.hips_tile_width, s_out)
  cd = G1(params); tiles = TileCache(order_sel)        # LRU 按 (ipix_tile)
  parallel for row_band in rows(out):                  # worker pool by affinity, 禁硬编码线程数
    if cancelled(row_band): return CANCELLED           # 行带粒度
    for y in row_band:
      for x in 0..W_out−1:
        (RA,Dec) = G2(cd, x, y)
        ipix = G4.map(RA,Dec, leaf_order)
        vals = tiles.gather(ipix, sampler)             # nearest 1 tile / bilinear ≤4 tiles
        S[y][x] = sample(vals, sampler); C[y][x] = all_finite_path(vals)
  write_fits_atomic(S, C, cd, provenance)              # ALG-P3-004
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| 缺 tile 文件 | 该足迹 C=0, S=NaN, provenance 记 missing, 不中断 |
| tile 内 NaN | S=NaN, C=1(mask 语义=coverage+NaN 判定) |
| RA wrap 0/360 | 球面角差归一, 无接缝 |
| 中心距极点 <5° / 输出跨 TAN 半球 | 显式拒(G2 前) |
| properties 非法/缺键 | 显式拒(ALG-P3-001), 无 silent default |
| JPEG/PNG/int+BLANK/多通道 tile | 显式拒(alpha 范围) |
| pixfrac/单帧参数 | 不适用(N/A), 拒绝字段 |

## 5 确定性与归约

- 逐输出像素独立；bilinear 权重由 leaf 邻域几何唯一确定(无迭代、无重结合)；`order_sel`/`cd` 由参数唯一决定；tile cache 只读(值路径与 cache 命中与否无关——同一 `sample_impl`)。

## 5c SIMD 安全与取消点

- `G2` 内为逐像素标量三角算术(自动向量化安全: 无跨像素依赖)；`S/C` 写入行连续无别名；bilinear 权重和=1 由构造保证(4 权重显式归一, FP64)。
- 取消点: 输出行带粒度(ALG-P3-003 循环)；取消时**输出文件不落盘**(tmp 删除, rename 不发生)——FITS 原子性以整文件为单元(ALG-P3-004)。

## 6 时间/空间复杂度

- 时间 O(W_out·H_out·(map+sample))；map O(1)(HEALPix ang2pix), nearest O(1), bilinear O(4)+cache 命中 O(1)；
- 空间 O(W_out·H_out) 输出 + O(cache_tiles·W²) tile 缓存；manifest/provenance O(tiles_used)。

## 7 CPU-only 后端策略（V5）

- 仅 CPU：行带 worker pool（按 affinity 调度, **禁止硬编码线程数**）；输出与线程划分无关(逐像素独立+固定序)；无 ISA 变体分支需求(三角函数经 libm, 结果确定性由同 libm 版本冻结, 跨平台数值合同入 SYN-007)。

## 8 参考实现/Oracle

- reference 实现即生产实现(首版)；Oracle=SCI-P3 §11 全集, **Oracle 不调用本模块**（独立小规模球面 reference + 独立 FITS/WCS 读取器）；容差: WCS roundtrip ≤1e-6 px, 常数场 max_abs=0(nearest), bilinear 常数场 max_abs=0, 解析场容差由 SYN-007 预冻结。

## 9 容差来源

- WCS roundtrip 1e-6 px：SCI-P3 §7 不变量(FP64 反向映射+Paper I/II 语义)；
- 常数场 0：bilinear 权重和=1 构造保证；
- 解析球面场容差：h≤s_out 约束下 bilinear O(h²) 误差界 → SYN-007 表冻结(任务 SYN-007 落实具体数值)。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md`(worker pool/取消) `PERFORMANCE_BUDGET.md`(P3 预算)
- API: `API-004`(CLI JSONL 输入/输出, 待建)
- TST: `SYN-007` 五件套(oracle 独立性)；`TST-P3-*` 编号随 API-004 建立
