# 命名块生命周期合同（CONTRACT-501 / RELEASE-05）

> 上游：ASTROCS_DESIGN.md §8.1（三命令独立进程、独立调度器；跨阶段只走磁盘产品 + manifest + 哈希）、§8.2（阶段内命名块内存管线与块生命周期）
> 依据：ENGINEERING_SPEC.md §4.1（管线纪律）

> ID: CONTRACT-501-BLOCK  状态: FROZEN  机器 schema: `eng/contracts/schemas/pipeline_block.schema.json`
> 端口事实源: `lib/infrastructure/pipeline/module_ports.registry.json`（v2，冻结绑定表）

## 1 模型

每个阶段内部是一条**内存管线**：`PipelineFrame` 承载一组**命名块（block）**；模块从帧读入参块、产出新块写回、显式声明**消费**的旧块；调度器按生命周期即时回收，模块不私藏大块数据的长期副本，**不跨阶段共享内存**。

### 1.1 载体合同（生产链路的真实载体）

命名块是**阶段内单节点执行期**的内存概念：`PipelineFrame` 承载的块不落盘、不跨节点、不跨阶段。生产链路上节点间交换的数据**不是**命名块，而是磁盘产品：

- **阶段内**：唯一载体是 `output_dir` **文件约定**。注册表里每个端口对应 `output_dir` 下的一个具体产物（文件或产品目录），生产者按该路径写、消费者按同一路径读；
- **跨阶段**：唯一载体是 **HiPS 产品树**（磁盘目录 + manifest + 哈希）。`normalize` 产出逐帧 HiPS 树，`mosaic` 产出马赛克 HiPS 树，`export` 只读马赛克 HiPS 树；
- 端口到载体的对应由注册表的 `carrier` 字段声明，取值 `output_dir_file` / `hips_product_tree` / `config_path`（阶段外部输入，本阶段无生产者）。同一产物身份不得在一个阶段产出、在另一个阶段被消费（跨阶段只能走 HiPS 产品树）。

机器判据见 §7。

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
| `lifecycle` | enum | `short`（节点内即时消耗）/ `frame`（帧内跨节点）/ `run`（长生命周期：活到本阶段末） |
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
- 阶段终产物以**磁盘产品**形态发布、不占运行期块：逐帧 HiPS 树 + 产品清单（normalize）、马赛克 HiPS 树 + 马赛克清单（mosaic）、投影 FITS + 校验报告（export）；
- **一次运行的峰值内存由在途块集合与分块大小决定**，不由总图大小决定（ARCH-503/504 的验收判据）。

## 6 负例（必须能红）

- 消费不存在的块 / 重复生产 / 生命周期声明不一致 ⇒ 构建期报错；
- 退化面（无合格数据）不得挂帧（不得创建空块冒充存在）；
- 取消路径泄漏（块未销毁）⇒ 内存归还计数判红；
- 注册表声明的边在代码中无对应读写（端口锚点解析不到）⇒ 判红（§7 C2）；
- 代码中真实存在的数据流未在注册表声明 ⇒ 判红（§7 C3）；
- 端口方向与代码读写角色不符、或角色无法由代码证据确认 ⇒ 判红（§7 C4）；
- 节点触碰 HiPS 产品树却无对应 `carrier` 端口、或同一产物身份跨阶段出现 ⇒ 判红（§7 C3b/C5）；
- 注册表为空、或端口数/代码 token 数/边数低于下界 ⇒ 判红（§7 C6；恒真比较无证据资格）。

## 7 机器判据（端口 ↔ 代码双向一致）

`eng/tools/quality/check_block_flow_ports_vs_code.py` 把注册表的端口面与
`lib/infrastructure/scheduler/src/module_adapters.cpp` 的真实数据流做**双向**核对：

| 判据 | 内容 |
|---|---|
| C1 结构 | 注册表 v2 + `carrier_contract`；每 module 恰一个 operation；端口 `name/direction/carrier/artifacts/code` 齐全 |
| C2 声明⇒实现 | 每条端口的 `code` 锚点必须解析：文件存在、`symbol` 在本文件内唯一解析到函数体、`token` 落在该函数体内 |
| C3 实现⇒声明 | 以**代码侧闭合文法**（与注册表无关）抽出每个节点函数触碰的产物 token，必须全部已在注册表声明 |
| C3b 载体一致 | 节点触碰 HiPS 产品树 ⇒ 必须有对应 `carrier=hips_product_tree` 且方向一致的端口 |
| C4 方向一致 | 声明 token 必须在代码里出现，且变量流分析推断出的读写角色必须包含声明的 `direction`；推不出角色即判红 |
| C5 载体合同 | 节点间端口 `carrier` ∈ {`output_dir_file`, `hips_product_tree`}；`output_dir` 产物身份不得跨阶段；`carrier_contract` 必须显式声明 HiPS 产品树为跨阶段载体 |
| C6 非退化 | 模块数/端口数/代码 token 数/生产→消费边数均有下界；空注册表判红 |

`--self-test` 给出 1 条正例（仓库现状必绿）与逐条负例（声明不存在的边、漏声明真实流、方向写反、锚点 token 删除/移出符号/符号不存在/文件不存在、跨阶段边、载体合同缺失、端口无锚点、节点函数改名、空注册表），负例必须逐条判红。
