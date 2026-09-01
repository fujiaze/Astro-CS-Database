# P3-004: 独立球面重采样 Oracle

任务 ID: P3-004
Gate: G6
依赖: P3-003
平台: Linux
变更类别: validation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P3-004：

> 不要在测试里重写普通矩形 bilinear 来冒充 HEALPix。使用独立高精度球面生成器/
> 标准库建立常数球、解析球面函数、impulse、跨 tile 连续场、RA 0/360、旋转 TAN、
> 边界 missing/NaN。生产 nearest/bilinear 逐 pixel 对照独立 WCS+HEALPix reference。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 常数球 | FIELD 常数场 → 输出恒定(±1e-3) | c01 #1 |
| 解析球面函数 | ANALYTIC.hips(逐像素 cos²dec) vs 独立 TAN+cos² reference(1% 内, nearest 像素粒度) | c01 #2 |
| 独立 WCS reference | indep_world(标准 FITS-TAN 公式, 独立于生产 p3_wcs; 中心 dec=45 验证) | c01 #2 |
| 跨 tile 连续场 | bilinear 输出相邻像素跳变 <0.05(无人工接缝) | c01 #3 |
| RA 0/360 | 中心 RA=0 覆盖正常(无无覆盖带) | c01 #4 |
| 旋转 TAN | east_right parity(CD1_1>0) 输出有效 | c01 #5 |
| 边界 missing/NaN | 既有 P3-001(read_leaf 缺 tile → coverage=0, NaN 传播) | 既有 |
| 不重写矩形 bilinear | Oracle 用独立球面 TAN+cos² reference, 不冒充 HEALPix | c01 |

## 实现文件

- `tests/backend/phase2_fixture_main.cpp`：新增 `--make-analytic`(逐像素 cos²dec 解析场 HiPS, leaf_nside=512 修正)
- `tests/backend/test_p3004_spherical_oracle.py`（新）：5 组断言(常数/解析场对照/无接缝/RA wrap/旋转 TAN)
- `tests/backend/test_p2003_seam_oracle.py`：incs 补 healpix 路径(新 fixture 依赖)

## 测试结果

- `test_p3004_spherical_oracle.py`: 5/5 PASS
- `test_p2003_seam_oracle.py`: 4/4 PASS; `test_p3_resample.py`: 6/6 PASS(回归)

## 说明

- 关键调试: ANALYTIC fixture 的 leaf nside 应为 512(2^9, tile order=0) 非 512<<9;
  独立 TAN reference 公式修正(dy 项 sin(c) 非 cos(c))后与生产 0.02% 一致。
- nearest 像素粒度量化误差 ~0.3%(tile 0.18°/px), 阈值 1% 严格且反映物理语义;
  bilinear 才达 1e-3。
