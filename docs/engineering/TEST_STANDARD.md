# 测试标准

> 上游：`docs/ASTROCS_DESIGN.md` §12.1（科学正确性与三重佐证）、§12.4（验证层级与四层验收）、
> §8.5（模块与 ABI：共址测试）
> 测试矩阵与逐条用例清单：`docs/engineering/TEST_MATRIX.md`

本标准规定测试的覆盖判据、确定性要求、命名与跳过规则、覆盖率分母与回归集合。
每个独立可调度模块的共址测试（单测、合同、负例）随模块在位，由该模块的注册面声明。

## 1. 覆盖判据

- 每条科学契约至少配 1 个 test / oracle（property、oracle 或 Monte Carlo 三选一）；
- 每个度量具备非退化判据，恒真门没有证据资格——真值无效应时度量必须归零或报警；
- 测试必须使用公共生产 API；实现面只有生产一份；
- 三类实验数据（仿真成像、解析代数合成、真实数据）各有独立用例面，
  不以某一类数据代表全部科学正确性。

## 2. 确定性

- 固定 seed；同一输入重复运行逐字节同结果；
- 浮点断言用容差 + 说明容差来源；容差在写测试前冻结；
- NaN / Inf / 缺失的位置与语义与产品逐项精确一致；
- 并行度不改变科学产品：1 worker 与 N worker 在冻结容差内等价，
  逐位等价的锚点见 `docs/engineering/execution_options_contract.md` §5。

## 3. 命名与跳过

- gtest 命名：`Suite.Feature`；
- GPU 或硬件能力不可用时用 `GTEST_SKIP`，不得以伪通过掩盖未执行；
- 负例必须可红：注入缺陷时判红，正确实现时判绿。

## 4. 覆盖率

覆盖率分母明确：shipping 单元与测试单元各自独立计数，不合并成一个数字。

## 5. 回归集合

以下集合为长期回归项，其判据骨架固定，实测数值与逐档读数归实验单元与
`docs/engineering/TEST_MATRIX.md`：

| 回归项 | 判据 |
|---|---|
| HiPS 序列化 | 与外部实现（`hipsgen`）对拍，逐成员一致 |
| 阶段间产品交换 | normalize 输出（signal / support / MOC / SNR / quality / frame_id / manifest）被 mosaic 与外部标准一致读取 |
| UPM 权重 | 并行度改变时权重归一结果逐位等价 |
| 原子发布 | 注入磁盘写满 / 写失败时无完成标记，产物可恢复 |
| 路径与词法 | 路径穿越、符号链接、非法文件名一律判红 |

## 6. 测试入口

| 入口 | 用途 |
|---|---|
| `lib/infrastructure/aio/tests/` | tile mapping oracle、sanitize、原子发布负例 |
| `eng/build/toolchain.ps1 check` | 环境与编译验证 |
| `eng/build/toolchain.ps1 build` | 构建验证 |
| `eng/tools/docs_machine_consistency.py` | 文档与源码的机器一致性判据 |

测试目标随模块在位，命名与目标映射由 `docs/engineering/BUILD_GRAPH.md` 声明。

## 7. 变更纪律

变更模块必须重跑该模块的相关 gate；引用未变更模块的既有产物时，
必须以构建指纹（内容 hash）证明同源，否则重跑。
受影响验证范围由改动集算出，只扩大不缩小。
