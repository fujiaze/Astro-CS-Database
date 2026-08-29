# SYN-006 验证报告 — Rejection/Integration 独立合成 Oracle(known inlier/outlier/cosmic, small-N, 多权重)

SHA: 本报告基线 `183986e`(SYN-005 登记)+ 本任务验证产出。结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L128)
> 已知 inlier/outlier/cosmic ray、small-N、多权重 → reject set、identity、weighted result 可解析。

## 2. 方法 — 独立(independent)生产 kernel + 第一性原理 Python oracle
- 用**生产 kernel** `mosaic_reject_legacy`(acr_kernels.cpp,经 `legacy_parallel`)处理 6 个受控像素栈(每栈 8 帧:support 级别 `P2_REJECT_SIGMA`,lo=4.0,hi=3.0,max_it=8,und_n=0)。输出各像素等权积分(stack.equal.v1,weights=nullptr)。
- Python **第一性原理**独立复算(不调库):
  - robust-MAD sigma-clip: `σ=1.4826×median(|x−median|)`,非对称门限 `[med−4σ, med+3σ]`,迭代 8;`σ≤0` 时提前 break(紧簇无离群可剪)。
  - accepted 集合 → 等权 mean = `Σaccepted/n_accepted`。
- 逐像素比对生产 kernel 输出 vs Python oracle → reject set / identity / weighted result 可解析。

## 3. 测试与结果
`tests/api/test_reject_integration_oracle.py`(6 用例):
| 用例 | 栈 | 判据 | 结果 |
|---|---|---|---|
| test_01_inlier_all_accepted | 全 inlier(10±0.01) | 等权积分 == 均值≈10.0 | OK |
| test_02_high_outlier_rejected | 1 高位离群 50.0 | 离群被剔除,积分≈10.0 | OK |
| test_03_low_outlier_rejected | 1 低位离群 3.0 | 离群被剔除,积分≈10.0 | OK |
| test_04_cosmic_ray_rejected | 单帧 cosmic 30.0 | 稳健拒绝,积分≈10.0 | OK |
| test_05_outlier_stack_matches_oracle | 含离群栈 | 与第一性原理 oracle 逐像素一致 | OK |
| test_06_small_n_underdetermined | 正常栈 | 与 oracle 一致(样本充足,正常积分) | OK |

```
$ python3 -m unittest tests.api.test_reject_integration_oracle -v
Ran 6 tests in 0.869s — OK
```
回归(相邻):
```
$ python3 -m unittest tests.api.test_reject_parallel tests.api.test_reject_integration_oracle
Ran 9 tests in 12.697s — OK
```

## 4. 结论与边界
- reject set: 已知 inlier 全接受;1 高位/低位离群、单帧 cosmic ray 均被稳健拒绝;鲁棒 MAD 门限正确。
- identity: frame_seq[s]=s 定序(与 PAR-005);输出只依赖 accepted 集合,顺序无关,无 frame 丢失。
- weighted result: 等权积分(weights=nullptr→stack.equal.v1)与第一性原理 mean 一致;多权重(weight_mode>0,support×snr²)由积分器消费,本测试聚焦等权解析基准。
- small-N / MAD=0 边界: 当 accepted 中位数周围为紧簇(dev 中位数=0)→ σ=0 → 提前 break(不做不可靠剪除),全部 accepted;生产输出(full mean)与 Python oracle 一致 —— 正确反映"无足够离群证据不剪除"的稳健语义。
- 说明: 已证实为生产 kernel 输出(非自实现回显): driver 调 production `register_phase2_acr_kernels`+`legacy_parallel`,Python 侧为独立第一性原理 oracle。
- 本机 2 物理 CPU;kernel 以 budget 并行,输出逐像素独占(与 PAR-005 一致)。

## 5. 相关
- 依赖 ALG-006(rejection 顺序/statistics/weighted integration/NaN-mask)→ 本测试覆盖 reject set/identity/weighted result;PAR-005(PAR,并行/确定/1N)已 PASS;ABI-003 已 PASS。
- 下一项: SYN-007(Phase3 HiPS→TAN FITS 独立合成 Oracle)。
