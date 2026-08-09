# Third-Party Notice — Shared HEALPix Core

## 算法来源

`healpix_core.cpp` 中的 `ang2pix_nest` / `pix2ang_nest` 算法
逐文件审核后迁移自 **astrometry.net** 的 `healpix.c`
(ang2pix 方向: `xyztohp` + `healpixl_xy_to_nested`;
pix2ang 方向: `healpixl_nested_to_xy` + `hp_to_xyz`),
该文件为 **BSD 3-Clause** 许可 (文件头声明:
`Licensed under a 3-clause BSD style license - see LICENSE`)。

同一份 BSD 实现也被 **astropy-healpix** 以 `cextern/astrometry.net/healpix.c`
的形式内置 (astropy-healpix 为 BSD-3-Clause 项目), 因此本项目以
astropy-healpix 作为独立 Oracle (1,000,000 全天随机点 + 锚点, mismatch=0)
验证本实现逐点一致。

## 本实现与上游差异

- 仅迁移 NESTED 排序所需路径, 未迁移 RING 排序与邻居查询;
- 极冠分支的 `coz` 直接由单位向量模长计算, 不依赖外部 `hypot` 约定;
- 位交错 `xy_to_nest` / `nest_to_xy` 为等价重写;
- 未复制任何 GPL (Healpix_cxx / RELION) 代码进入生产树。

## 外部验证依赖

| 组件 | 版本 | 许可 | 用途 |
| --- | --- | --- | --- |
| astropy-healpix | 1.0.3 (测试环境) | BSD-3-Clause | 全天空 Oracle |
| astropy | 7.1.0 (测试环境) | BSD-3-Clause | Oracle 单位/坐标封装 |

以上仅用于测试, 不进入生产 DLL。
