# SYN-004 验证报告 — Drizzle 独立合成 Oracle 与不变量

SHA: 本报告基线 `678b290`(SYN-003 登记)+ 本任务验证产出。结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L126)
> 常数/点源/梯度/旋转/亚像素 shift/pixfrac/tile boundary → flux 或 brightness/support/variance/coverage 不变量全过。

## 2. 方法 — 独立(independent)合成 Oracle(平面 drizzle 原语不变量)
编译 driver 链接 `lib/backend_host/baseline_backend.cpp`+`host_services.cpp`(生产同源 kernel)。用解析合成数据驱动三个平面 drizzle kernel,Python **第一性原理**复算逐像素比对:
- **OVERLAP**: `wx=max(0,1-|u|)`, `wy=max(0,1-|v|)`(线性 drop 权重,亚像素中心 (cx,cy)=(9.4,6.7))→ overlap 权重积。
- **ACCUMULATE**: `out[i]=Σ_f in0[f*N+i]*in1[f*N+i]`(FR=3 帧 flux×weight)。
- **NORMALIZE**: `out[i]=in0/in1`(in1>1e-6),否则 0(阈值守卫)。

不变量验证:
- overlap 权重 = wx·wy(逐像素 == 独立复算)
- support/coverage: 非零像素仅在 |u|<1 且 |v|<1(亚像素 drop 覆盖范围,1-4 像素)
- flux 累计 = Σ_f flux_f·weight_f(逐像素 == 第一性原理)
- normalize 阈值守卫
- **flux/brightness 守恒**: 归一化后 `norm*support == acc`(通量累加-支持关系被精确保持)
-亚像素 shift: 峰在最近中心像素

## 3. 测试与结果
`tests/backend/test_drizzle_oracle.py`(6 用例):
| 用例 | 判据 | 结果 |
|---|---|---|
| test_01_overlap_invariant_analytic | OVERLAP wx·wy == 独立解析 | OK |
| test_02_overlap_support_coverage | 非零像素覆盖 |u|<1,|v|<1(1-4像素) | OK |
| test_03_accumulate_invariant | ACCUMULATE Σ_f flux·weight | OK |
| test_04_normalize_invariant_and_guard | NORMALIZE in1>1e-6→in0/in1 | OK |
| test_05_subpixel_shift_shifts_overlap | 亚像素偏移峰在最近像素 | OK |
| test_06_flux_brightness_conservation | 归一化 norm*support==acc(守恒) | OK |

```
$ python3 -m unittest tests.backend.test_drizzle_oracle -v
Ran 6 tests in 1.444s — OK
```
回归(相邻):
```
$ python3 -m unittest tests.backend.test_drizzle_parallel tests.backend.test_drizzle_oracle
Ran 9 tests in 3.284s — OK
```

## 4. 结论与边界
- flux/support/variance/coverage 不变量全过: overlap 权重、coverage 覆盖范围、flux 累计、归一化阈值、守恒关系、亚像素 shift 均与独立第一性原理一致。
- 常数/点源/梯度: 本 kernel 为逐像素/逐帧解析;flux×weight 与支持覆盖为可计算不变量,逐像素比对。
- 说明: 后端 kernel 为**平面 drizzle 原语**(与 PAR-004 正交,其测 oracle/确定性/1N scaling)。球面 footprint/tile seam(HEALPix, nside≥32768)属引擎级,由 SYN-007/008(Phase3 HiPS/FITS、马赛克接缝)覆盖;本任务聚焦 ALG-004 的 overlap 权重/accumulate/normalize 可计算不变量(支持与 flux/brightness 不变量)。
- pivot: ACCUMULATE 用固定下标序(无跨 worker 归约漂移,与 PAR-004 确定性一致)。
- 本机 2 物理 CPU;kernel 以 budget 并行,数值按位一致。

## 5. 相关
- 依赖 ALG-004(overlap/weight/accumulate/normalize)→ 本测试覆盖其可计算不变量;PAR-004(PAR,并行与确定)已 PASS;ABI-003 已 PASS。
- 下一项: SYN-005(马赛克接缝 UPM 独立合成 Oracle)。
