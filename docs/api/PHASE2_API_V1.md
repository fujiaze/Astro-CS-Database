# Phase2 API 定义 v1 (API-004 冻结 — 数据所有权/thread budget/逐函数)

> ID: API-P2-001  范围: API-P2-001..012  状态: FROZEN (V5 API-004, 2026-08-28)  上游: API-001/002/003  下游: CLI-002(phase2 run handler)/TST-P2-*
> 签名权威=现存头文件(lib/phase2/include/astro/phase2/*.h);本文件登记并发合同、**数据所有权**与 thread budget 绑定;禁止隐藏全局状态(验收)。

## 1 阶段流水与所有权图(谁分配/谁持有/谁释放)

```text
coverage ──→ sampler ──→ UPM build ──→ calibrate_block ──→ rejection ──→ integration
(Owned: 调用方 session)   (Owned: UPM Model 对象)    (borrow model)    (borrow stack)  (值语义 out)
```

| 对象 | 创建 | 持有 | 释放 | 跨函数传递 |
|---|---|---|---|---|
| `Coverage`(p2_coverage_build) | build | 调用方 | p2_coverage_free | 只读借用 |
| ControlObservation[]/frame_id_cache | 调用方(p2_sample_controls 产出) | 调用方 | 调用方 | 只读借用(cached 版免二次哈希, 同数值语义) |
| UPM Model(void*, p2_upm_build / p2_upm_build_geo / p2_upm_open) | build/open | 调用方 | **p2_upm_close** | calibrate_block/evaluate_c 只读借用;persist 导出副本 |
| CandidateStack(p2_collect_candidate_stack) | collect | 调用方 | 调用方(strided 视图者不 free 底层帧) | rejection/integration 只读借用 |
| P2PixelStack→P2PixelResult | 调用方栈/结果 | 调用方 | 调用方 | p2_integrate_pixel(in const, out 值写) |
| async_io 队列(CON-008) | session | session | destroy | bounded(深度冻结), 无无界缓冲 |

- **无隐藏全局状态**: 唯一模块级资源=g_model_floor(指针 key 注册表, 单线程资源, ARCH-004 已登记)+logger(宿主注入);其余全部经参数/handle 传递。

## 2 逐函数登记(签名权威=头文件;并发五字段+test ID)

| 函数 | reentrant | threadsafe | internal_parallel | 取消点 | test ID |
|---|---|---|---|---|---|
| `p2_coverage_build/free` | yes | no(独立对象) | none | 无 | TST-COV-* |
| `p2_sample_controls` / `p2_sample_controls_cached` | yes | no | none(串行=确定性 reference, ARCH-004) | cell 粒度(实验并行) | TEST-P2SAMPLE-* |
| `p2_sampler_default_config` | yes | yes | none | 无 | TEST-P2SAMPLE-* |
| `p2_upm_build` / `p2_upm_build_geo` | yes | no | 块级(worker budget, 固定 control 序) | 整模型(不写半成品) | TEST-UPM-* |
| `p2_upm_calibrate_block`/`p2_upm_evaluate_c` | yes | yes(模型只读借用) | block 内 none | 无(短任务) | TEST-UPM-* |
| `p2_upm_open`/`p2_upm_close`/`p2_upm_info` | yes | no(模型对象) | none(persist IO 串行) | 整模型 | TEST-UPM-* |
| `p2_reject_plan_resolve` | yes | yes | none | 无 | TST-REJ-* |
| `p2_eligibility_filter`/`p2_collect_candidate_stack` | yes | yes | none(收集器 strided) | 无 | TST-REJ-* |
| `p2_validate_candidate_weights` | yes | yes | none | 无 | TST-REJ-* |
| `p2_reject_stack` / `p2_reject_stack_ex` | yes | no(per-sample reason 输出) | 像素行带 | 行带(掩膜帧原子) | TST-REJ-* |
| `p2_integrate_pixel` | yes | yes(纯函数) | none(像素内固定序) | 无(行带由调用方切) | TST-INT-* |
| `p2_large_scale_apply` | yes | yes | 邻域读行带 | 行带 | TST-REJ-* |
| `p2_frame_id` / `p2_stats_median` / `p2_stats_mad` / `p2_rejection_semantic_id` | yes | yes | none | 无 | TEST-P2SAMPLE-*/TST-REJ-* |
| `p2_acr_block_eligible`/`p2_block_plan` | yes | yes | none | 无 | 配置守卫(ACR-IVAR-001; V5: 非 cpu/auto 拒) |

## 3 thread budget 绑定(ARCH-004 实例化)

- phase2 run 预算分配冻结: sampler=1(串行 reference);upm build=blocks(budget);rejection/integration=行带(budget);async I/O=1;**Σ≤全局 budget**;每 stage_start 事件携带 workers 实际值(API-002 backend 事件)。

## 4 错误码映射

模块 rc(NO_DATA/INVALID_INPUT/UNDERDETERMINED/rc=2 build fail/OK, SCI-UPM/REJ/INT 冻结)→acs_status: NO_DATA/INVALID→ACS_ERR_PARAM;rc=2→ACS_ERR_STATE;UNDERDETERMINED→ACS_OK(语义=可继续, final 汇总);映射表由 CLI-002 落地并在 golden 测试断言。

## 5 与 API-003 同构 checker

tests/api/test_p2_api.py 机器门: 文档符号↔头文件实跑核对+所有权图完整(七对象)+禁止隐藏全局状态声明+预算绑定引用。
