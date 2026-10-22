# validation/cat — 目录模块与盲解的独立验证

本目录是 `src/catalogue` 与 `src/sources/astrometry*` 的独立验证。程序只通过公开
API（`include/acsd/contracts.hpp`）访问实现，不 include 生产树任何内部头；真值由
astropy/ERFA 独立给出。

## 组成

| 文件 | 作用 |
|---|---|
| `catalogue_check.cpp` | 目录索引、provider、页面、quad描述符、历元传播的自检；输出 `catalogue_check.json` |
| `astrometry_check.cpp` | 盲解的合成星场解算；输出 `astrometry_check.json` |
| `astrometry_check.py` | 盲解的 astropy 对拍（0.293″，容差 2″） |
| `epoch_check.py` | 历元传播的 astropy 对拍（18个样本） |
| `proper_motion_oracle.py` | 二十年历元差、0.4/2/4″/yr 自行传播的 astropy 对拍（容差 1e-5″） |
| `astrometry_oracle.json` | 盲解 astropy 对拍的冻结输入 |

## 跑法

```sh
cmake -S validation/cat -B build/cat -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cat
./build/cat/catalogue_check        # 写 validation/cat/catalogue_check.json
./build/cat/astrometry_check       # 写 validation/cat/astrometry_check.json
python3 validation/cat/astrometry_check.py
python3 validation/cat/epoch_check.py
python3 validation/cat/proper_motion_oracle.py
```

## 历元语义（合同要求，API.md/OBJECTS.md 同步）

- `catalogue.lookup`：**目录历元**的目录位置（源自身历元，不做观测历元传播）。
- `catalogue.query`：**请求历元**的预测位置（含 pmRA*cosDec、视差、已知 RV 的空间运动）。
- 盲解、PSF、测光构造位置假设必须用 `query` 的观测历元位置；用 `lookup` 的静态位置
  会把自行当成天测误差（历元差 20 年、pm 4″/yr 时约 80″）。
- 仿射假设与验证用的是**同一批位置**（观测历元的区域查询结果）。这不是循环论证：
  假设是拟合四个四边形位置的**仿射模型**，验证是该模型在全场的**残差**；
  独立性来自位置来源与模型形式不同，而不是来自位置不同。历元是观测历元，
  所以源的自行既不被重复计入，也不会被当成天测误差。

## 省略的改正误差界（F08）

目录没有观测者，发布的位置是重心坐标，省略的 topocentric 改正项列入
`assessment.omitted`，其界写入 `assessment.omitted_position_bound_rad`：

- 已知距离 ⇒ 界 = 该源的视差界（1 mas ⇒ 4.6e−5 rad ≈ 9.5″，远大于盲解 2″ 容差）；
- 无已知距离 ⇒ 界**不写**（无界），并把 `assessment.noise` 置 `unknown`；
  省略的误差**绝不写成 0**。

`catalogue_check.cpp` 对这两条都有断言（界已发布、界非零、界等于视差界）。

## 距离残差是模型量而不是误差（F09）

实现按**三维匀速直线运动** r(t)=r0+v·dt 传播，距离取传播后位置向量的模，
因此带切向运动的二阶项；参考实现（ERFA `pmsky`，经 astropy `apply_space_motion`）
用自己的规则更新距离，不含该二阶项。两者**方向一致**（角差：最差样本 3.4e−11″，
18 样本 ≤1.9e−5″；20 年高自行 3.1e−6″），距离的相对差 ≤ 5.5e−6
（实测 `epoch_check.json` 7.7e−7、`proper_motion_oracle.json` 4.1e−8）。
**距离差标为模型相关量，角位置才是判据。**

## 合同路径必须可跑（F05 的验收要求）

上表的程序走**进程内的公开 API**。合同路径（`apps/contract` 的 `acsd-contract`
进程）目前**没有**实现目录与盲解的公开操作，因此
`catalogue.lookup` / `catalogue.query` / `catalogue.query_quads` / `build_catalogue_index` /
`make_catalogue_provider` / `solve_astrometry` / `fit_psf` / `measure_science_sources` /
`integrate_response` / `fit_photometry` / `apply_scale` / `bind_wcs` /
`bind_observation_model` / `calibrate_observation` / `regroup_observation` 这 16 个操作的
**合同路径尚无实跑证据**。

验收要求（由 Lead 的 apps/contract 扩展 dispatch 落实）：

1. `apps/contract` 的 operation 白名单必须覆盖上述 16 个操作（当前白名单只有 24 项），
   否则合同路径不可达，等于这些模块没有合同级验证。
2. 每个操作至少有一条 `validation/cat/` 风格的请求/响应证据：请求进、响应出、
   退出码 0，且响应中的科学量与本目录的进程内证据一致。
3. 合同请求里的 `AstrometricContext` 必须带 `epoch_julian_year`；`epoch_julian_year`
   与目录历元之差就是自行引入的量级，跨 20 年时 pm 4″/yr 约 80″，
   合同路径的回归必须含这一量级。

在 `apps/contract` 覆盖这些操作之前，本目录的结论只对**进程内公开 API**成立，
不得据此声称合同路径通过。
