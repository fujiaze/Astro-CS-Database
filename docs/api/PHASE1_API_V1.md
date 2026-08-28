# Phase1 API 定义 v1 (API-003 冻结 — 逐函数 create/validate/run/inspect)

> ID: API-P1-001  范围: API-P1-001..010  状态: FROZEN (V5 API-003, 2026-08-28)  上游: API-001(API-COMMON-001)/API-002  下游: CLI-002(phase1 run handler)/TST-P1-*
> 模式: Phase1 = 现有 C ABI 模块链(calibration/star_detector/dynamic_psf/ipv/photometric_calib/snr_estimator/healpix_drizzle)的**编排合同**;每函数按五字段并发合同模板(API-001 §3)登记;此为 V5 冻结层,V4 既有函数签名以现存头文件为准(不重写,新增仅 orchestrator 侧)。

## 1 生命周期合同(编排级,CLI-002 handler 直调)

```c
/* 四段式: create→validate→run→inspect;opaque handle, owner=创建者 */
acs_status p1_session_create(const acs_allocator*, const acs_logger*, const acs_cancel*,
                             const acs_thread_budget*, acs_handle* out);          /* reentrant:yes; threadsafe:no(handle 级) */
acs_status p1_session_validate(acs_handle, const acs_span_u8 config_json);        /* 纯读; 无 IO; 幂等 */
acs_status p1_session_run(acs_handle, const acs_span_u8 config_json,
                          int async_io_depth);                                    /* async_io_depth∈{0,1,2}(ARCH-004 §2); 取消点=帧粒度 */
acs_status p1_session_inspect(acs_handle, acs_span_u8* out_manifest_json);        /* out=host alloc, 调用方释放 */
acs_status p1_session_destroy(acs_handle);                                        /* 唯一释放对; 内部 join 后台 IO 线程 */
```

- run 内部阶段序列=stages[](校准→检测/PSF→plate solve→测光定标→SNR→Drizzle→HiPS),与 production_call_paths_stage1.csv 的 7 路径一一对应;每 stage 发 stage_start/stage_end+backend 事件(API-002 §4)。

## 2 底层模块函数登记(现存头文件为签名权威;此处登记并发合同+测试 ID)

| 函数(头文件) | reentrant | threadsafe | internal_parallel | 取消点 | 直接 test ID |
|---|---|---|---|---|---|
| `ac_generate_master_bias/dark/flat(+_f64)`(astro_calibration.h) | yes | yes(无共享可变) | omp(budget, pixel 域) | 无(短任务) | TST-CAL-001 |
| `ac_calibrate_frame(+_f64)`(同上) | yes | yes | omp(budget, 行带) | 行带 | TST-CAL-001 |
| `ac_correct_frame(+_f64)`(同上, cosmetic) | yes | yes | omp(坏点域) | 无 | TST-CAL-FAIL-001 |
| `ac_set_num_threads(int)`(同上) | yes | yes | — | — | TB-ARCH-004(checker 管控; **V5 迁移整改点**: 由 p1 budget 注入取代, ABI-001 收编) |
| `sdet_create/destroy/detect/detect_ex`(star_detector.h) | handle 级 no | no(单 handle 单线程) | omp(星批) | 星批 | TST-SDET-* |
| `dpsf_fit/batch/batch_f/free_results`(dynamic_psf.h) | yes | yes | omp(星批) | 星批 | TST-DPSF-* |
| `ipv_solve_create/destroy/solve(_from_memory)`(ipv_api.h) | handle 级 no | no | omp(triangle/vote, 帧内) | 帧(星表行块) | TST-IPV-001 |
| `pc_calibrate_simple(_with_gaia)`(photometric_calib.h) | yes | yes | none(IRLS 串行确定性) | 迭代间 | TST-PHOT-001 |
| `snr_noise_model_v1(+_f64/_fill/_free)/snr_noise_gain_variance`(snr_estimator.h) | yes | yes(model 对象隔离, g_model_floor 指针 key) | patch 级 | 行带 | TST-NOISE-001..015 |
| drizzle 引擎(healpix_drizzle) | 帧级 | no(帧序串行) | omp(候选/行带, 固定序归约) | 帧/tile | TST-DRZ-* |

- aliasing: 全部 in/out 不重叠(除标注 in-place 的 normalize_flat);错误码沿用各模块既有枚举(AC_ERR_*/sdet/dpsf/ipv/pc/snr),session 层映射至 acs_status(表由 CLI-002 落地)。

## 3 单位/所有权速查(引 GLOSSARY/COMMON_ABI)

- 尺寸: w,h(像素), n_frames;曝光: 秒;信号: ADU;方差: ADU²;坐标: 内部 0-based 像素;RA/Dec: deg ICRS。
- 内存: 全部"分配方释放或 host allocator"(函数头注释为准);handle 生命周期=唯一 create/destroy 对;借用(gaia_client_handle 等)不转移所有权。

## 4 doc-symbol-signature checker 合同(验收)

`tools/check_api_docs.py`(API-003 建立合同, CLI-002 落地全量): 对每个登记函数——① 头文件存在该符号;② 文档表此行存在;③ 签名(参数数)一致;④ 直接 test ID 非空;⑤ 五字段并发合同齐全。任一缺失 FAIL。本任务先以 §2 表+tests/api/test_p1_api.py 机器门立约。
