# reverse_verify 定案结论 · SCI-C（UPM 控制点平滑 λs）

> 上游：ASTROCS_DESIGN.md §2.3 / §12.3；`ENGINEERING_SPEC.md §9`
> 来源：`reverse_verify/docs/smooth-lambda.md`（2026-09-21 ROOT-CONSOLIDATION 迁入 `实验/additive-sky-seamless/docs/smooth-lambda.md`）

## 1. 现行结论（原文 §0 结论速览）

> **诚实登记**：原文 §0 表格的「关键数值」列留的是 `X`/`Y`/`N` **占位符**，
> 指向 §3.2/§3.3/§3.5/§4/§6/§7 —— 即该文档的**结论速览从未回填**。
> 本节照实登记，不代为编造数字；可用数值见原文对应小节。

| 问题 | 结论 | 关键数值 |
|---|---|---|
| λs 能否有效去除加性天光？ | **能，但存在明确上界**：λs 超过 ~0.1 后 C 场被压平，逐帧天光梯度扣不掉 | 见原文 §3.2 |
| λs=1000 是不是「合适值」？ | **不是**。λs=1000 已是「C 场≈逐帧常数」的**过平滑极端**（≡ λs→∞） | 见原文 §3.2 |
| 亮区会不会被压暗？ | 见原文 §3.3 / §5 | 峰区压暗 = X%（原文未回填） |
| 负责人的缓解观察（各帧都亮 ⇒ 问题不大）成立吗？ | 见原文 §3.5 | 一致 vs 帧间抖动：X% vs Y%（原文未回填） |
| 真实 L4 台阶消除？ | 见原文 §4 | 台阶比 X（λs=0）→ Y（推荐 λs）（原文未回填） |
| 推荐生产默认值 | 见原文 §6 | `upm.smoothing_lambda` = **X**（区间 [a, b]）（原文未回填） |
| 是否需要自适应 λs？ | 见原文 §7 | — |

> **单位声明（逐字保留）**：本分片所有判据只用**尺度无关**的相对量（比值 / 相对偏差）。
> 合成场景里的 gain / read noise / dark current 是**正向物理模拟参数**，**不是**从 FITS 头
> 或数据反推的物理量；真实数据一律称「流水线标定单位」，不赋予物理量纲。
> λs 的最优值对绝对标度不变（判据 C7 数值验证）。

## 2. 状态

**待完成**：原文 §0 结论速览的占位符回填（属 SCI-C 实验单元的后续工作，不在本次根目录整合范围）。
本次迁移**只搬位置、不改结论**。

## 3. 复跑

```bash
bash 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/build.sh          # 链接真实 upm.cpp，零 ninja/cmake
bash 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/gen_all.sh        # 生成全部合成场景
SW_LAM="0,1e-3,0.01,0.03,0.1,0.3,1,10,1000" bash 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/sweep_all.sh <场景名...>
bash 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/run_mosaic_ls.sh <lambda> <tag> [n_frames]
```

实测与退役登记见 `run/ROOT-CONSOLIDATION/logs/migration_rerun.md`。
