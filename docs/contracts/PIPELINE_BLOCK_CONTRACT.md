# 命名块生命周期合同（CONTRACT-501 / RELEASE-05）

> 上游：ASTROCS_DESIGN.md §8.1（三命令独立进程、独立调度器）、§8.2（阶段内命名块内存管线与块生命周期）
> 依据：ENGINEERING_SPEC.md §4.1（管线纪律）

> ID: CONTRACT-501-BLOCK  状态: FROZEN  机器 schema: `eng/contracts/schemas/pipeline_block.schema.json`

## 1 模型

每个阶段内部是一条**内存管线**：`PipelineFrame` 承载一组**命名块（block）**；模块从帧读入参块、产出新块写回、显式声明**消费**的旧块；调度器按生命周期即时回收，模块不私藏大块数据的长期副本，**不跨阶段共享内存**。

## 2 块元数据（冻结字段）

| 字段 | 类型 | 语义 |
|---|---|---|
| `name` | string | 块名（阶段内唯一，小写蛇形） |
| `shape` | int[] | 维度（像素域块为 [h,w] 或 [n]） |
| `dtype` | enum | `f64` / `f32` / `i32` / `u8` |
| `unit` | string | 物理单位（ADU / ADU² / e⁻ / pixel / 无量纲 / null） |
| `optional` | bool | 可缺性：true 时缺失必须走**显式降级声明**，不得静默 |
| `producer` | string | 生产者节点 ID（有且仅有一个） |
| `consumers` | string[] | 消费者节点 ID 集合（可空 ⇒ 终态块） |
| `lifecycle` | enum | `short`（节点内即时消耗）/ `frame`（帧内跨节点）/ `run`（长生命周期到导出） |
| `provenance` | object | 头部 KV（随块流转，见 §4） |

**生命周期 DAG 校验（构建期，非法即报错）**：
1. 每个被消费的块**有且仅有**一个生产者；
2. 消费不存在的块 ⇒ 非法；
3. 同一块被重复生产 ⇒ 非法；
4. 声明的 `lifecycle` 与实际消费者跨度不一致 ⇒ 非法。

## 3 状态机（创建 → 消费 → 销毁）

```text
CREATED --(所有声明消费者执行完毕)--> CONSUMED --(调度器回收)--> DESTROYED
   |                                     ^
   +--(帧取消 / 异常)---------------------+--> DESTROYED（无泄漏路径）
```

- 块被**全部声明消费者**用完即销毁、内存归还（引用计数 = 剩余消费者数）；
- 阶段结束**不得**残留任何块（跨阶段只走磁盘产品 + manifest + 哈希）；
- 异常/取消路径必须与正常路径一样销毁（单测覆盖）。

## 4 provenance 流转

- 头部 KV（如 `frame_id`、`photometry_applied`、`k_photo`、`snr_path_effective`、`saturation_filter`）随块流转，下游不得丢弃；
- 降级必须**显式**：`optional=true` 的块缺失时，消费方须写 `degraded_reason`，禁止静默用缺省值。

## 5 生命周期与峰值内存

- 典型短生命周期：`raw` 在校准后销毁、`calibrated` 在测光归一化后销毁；
- 长生命周期：WCS / PSF / SNR / 星表匹配 存活到导出；
- **一次运行的峰值内存由在途块集合与分块大小决定**，不由总图大小决定（ARCH-503/504 的验收判据）。

## 6 负例（必须能红）

- 消费不存在的块 / 重复生产 / 生命周期声明不一致 ⇒ 构建期报错；
- 退化面（无合格数据）不得挂帧（不得创建空块冒充存在）；
- 取消路径泄漏（块未销毁）⇒ 内存归还计数判红。
