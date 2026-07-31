# 执行顺序与停止规则

## Phase 0：仓库就地审计

- 确认现有仓库根目录、构建系统、CLI入口、HISS代码、Browser入口、Wiki位置和测试入口。
- 找出以下内容：
  - Python-only HISS 原型；
  - 未接入正式 CLI 的“v2”试验代码；
  - 固定数组分块、RING、错误 pixfrac、未测光 signal 等冲突实现；
  - 仍要求每阶段停下的旧 Agent 文档；
  - Stage2 或 710 回归相关自动触发路径。
- 不创建新仓库。旧实现无价值时删除；仍有迁移参考价值时移入明确的 `archive/legacy_prototype/` 或在 Wiki 标记“仅历史参考，不是规范”。

## Phase 1：先冻结文档，不先写新算法

先把 `02_FROZEN_STAGE1_HISS_SPEC.md` 的已确认内容写入仓库 Wiki/规范：

1. Stage1 边界；
2. 三种校准模式与最优 Dark 回退；
3. PlateSolve 星点复用；
4. NSIDE 自动选择；
5. NESTED；
6. Gaia 测光校准后累计通量；
7. pixfrac 与球面重叠；
8. support；
9. Tile、自适应层级与占用编码；
10. 独立子块；
11. XISF 式单体容器；
12. 精简元数据与无 WCS；
13. 每子块 codec/transform 字段；
14. 未决实验不得自动定案。

文档未同步前，不允许继续沿用旧接口写实现。

## Phase 2：接口与数据契约落地

先统一接口，再并行实现无依赖部分：

- HISS schema/目录结构；
- Tile 与子块内存对象；
- Reader/Writer 接口；
- CLI Stage1 参数与错误码；
- Browser 读取接口；
- C++ benchmark 接口。

接口冻结后，可并行修改：

- Drizzle 与 Tile 累加；
- HISS Writer；
- HISS Reader；
- Browser；
- 测试；
- benchmark harness。

## Phase 3：实现所有已冻结内容

- 正式路径必须是 C++。
- Python 只允许用于非权威辅助，例如绘制报告图；不得用于测量核心压缩/解压性能，也不得成为 HISS Reader/Writer 的正式实现。
- 未决压缩默认值先通过可插拔接口和 RAW 基线实现，不写死最终选择。

## Phase 4：C++ 实验

严格按 `05_CPP_EXPERIMENT_PLAN.md` 执行。实验结论只能写为：

- 原始测量；
- 候选排序；
- 推荐方案；
- 风险和适用范围。

不得把“推荐”直接改成最终冻结默认值。

## Phase 5：正确性测试与性能报告

- 只运行代表性 Stage1 测试，不运行 Stage2 和 710 全量回归。
- 先证明科学语义和格式正确，再做详细性能剖析。
- 性能问题只报告；除明显局部低风险修复外，不擅自改变数学算法或科学语义。

## Phase 6：精简交付

按 `07_FINAL_DELIVERY_STRUCTURE.md` 交付，只包含必要变化和报告。
