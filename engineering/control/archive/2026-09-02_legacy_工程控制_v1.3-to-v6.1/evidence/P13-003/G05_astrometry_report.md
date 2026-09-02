# G5 外部 Oracle 补齐 — Astrometry.net（V4 最终签字包）

日期: 2026-08-09

## 环境

- WSL Ubuntu（gcc 15.2），apt 安装 astrometry.net 0.98 + Tycho-2 索引
  （index-tycho2-08/09/10-19，覆盖 30′ ~ 2000′ 视场）。
- 每个外部命令均以 `timeout 600/900` 包裹（控制包要求）。
- 只作外部 Oracle，未进入生产链/依赖。

## Synthetic（已知 WCS 合成场）

数据源: solve-field 对真实 T4 1024² crop 盲解后的全部检测源（.axy，3041 个，
保留真实 FLUX 层级），经真实解 WCS 反投影为天球坐标，再由**注入 WCS**
（真实解 WCS 施加 scale×0.97、rotation +13°、center +0.05°/-0.03° 扰动）
投影到新像素渲染（亚像素高斯 PSF，FITS 不含 WCS）。

盲解结果: `Field solved`，99 星匹配，log-odds 384.65。

| 指标 | 值 | 门 |
| --- | ---: | ---: |
| 中心角距 | 0.84″ | ≤2″ |
| 尺度相对误差 | 0.029% | ≤1% |
| 方位误差 | 0.13° | ≤0.2° |
| 像素→天球 median/p90/max | 0.56″ / 1.38″ / 2.14″ | — |
| 匹配星残差 median/p90/max | 0.54″ / 1.38″ / 1.76″ | ≤2″ / ≤6″ |

结论: 盲解恢复注入 WCS，尺度误差 <0.03%，中心/星残差亚角秒级。

## Real（T4 1024² crop）

- 带 WCS 副本: solve-field 验证既有 WCS 成功（124 星匹配，log-odds 828）。
- **剥离 WCS 后盲解**（`real_crop_blind.fits`）同样成功（31 星匹配，log-odds 179.7）。

与 AstroCS WCS 链（platesolve 日志 CRVAL + drizzle lineage 像素四角推导的
尺度/方位）对比:

| 指标 | 值 | 门 |
| --- | ---: | ---: |
| 中心角距 | 1.18″ | ≤5″ |
| 尺度相对误差 | 0.018% | ≤2% |
| 方位误差 | 0.018° | ≤0.5° |
| 像素→天球（4096 角点）median/p90/max | 0.97″ / 2.88″ / 6.08″ | ≤2″ / ≤10″ |
| 匹配星残差（31 颗 Tycho）median/p90/max | 0.64″ / 1.57″ / 9.97″ | — |

对照: AstroCS platesolve 自身 RMS=0.679″（n_pairs=53，Gaia），与 astrometry.net
匹配残差中位 0.64″ 一致；两套独立求解在中心/尺度/方位/像素→天球上互相印证。

## 过程中发现并修正的测试工具问题（不影响生产算法）

1. solve-field 的 `--scale-low/high` 默认按视场度数解释，须显式
   `--scale-units arcsecperpix`；
2. 合成渲染原先把星点取整到整数像素，丢失亚像素位置（simplexy 只测到取整中心），
   改为亚像素精确高斯溅射；
3. `.axy` 坐标为 FITS 1-based，astropy `pixel_to_world` 为 0-based，
   反投影天空前必须 `-1`（1px≈6.3″ 的系统偏差即由此产生）；
4. 对比脚本中心指标从“直接比 CRVAL”改为“同一图像中心像素（FITS 1-based ↔ 0-based
   换算）在两套 WCS 下的投影角距”。

工具脚本: `tools/astrometry_oracle/`（gen_synthetic_from_axy.py /
make_astrocs_ref.py / compare_astrometry.py，均 NON_PRODUCTION_TOOL_ONLY）。
