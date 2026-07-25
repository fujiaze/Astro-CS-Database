# 05 星点检测去重与 PlateSolve 保守迁移规范

## 1. 问题定义

当前路径中：

1. `ipv_solve_from_memory` 在 PlateSolve 内部把 float32 图像转换为 uint16，并调用 `sdet_detect_ex`；
2. 求解结束后 Orchestrator 又对同一图像做一次转换和检测，以生成 `star_det`；
3. PSF 消费第二次检测得到的坐标，并再次把 float32 图像转换为 uint16。

因此需要解决的是：同帧重复检测、重复量化、PlateSolve 与 PSF 使用不同星表的风险，以及无必要的全图临时缓冲。

## 2. 不可破坏的优先级

PlateSolve 是已积累真实数据验证的成熟模块。去重不能以降低板解算成功率或精度为代价。

优先级固定为：

1. 保持全量 TestData 的 PlateSolve 正确性和稳定性；
2. 每帧只执行一次星点检测；
3. PlateSolve 与 PSF 使用同一次检测产生的同一份 `star_det`；
4. PSF 直接处理 float32，消除第二份 uint16 全图缓冲；
5. 在不影响前四项的前提下再优化模块解耦和性能。

## 3. 两条可接受的数据路径

### 3.1 候选路径 A：上游显式检测

```text
CALIBRATE 后 data(float32)
        │
        ▼
STAR_DETECT（唯一调用 sdet_detect_ex）
        │ 产生 star_det v1
        ├──────────────► PLATESOLVE_FROM_DETECTIONS
        └──────────────► PSF
```

该路径模块边界最清晰，但只有在**全量 PlateSolve TestData 无退化**时才能成为生产路径。

### 3.2 保守路径 B：保留 PlateSolve 原始内部检测并导出结果

```text
CALIBRATE 后 data(float32)
        │
        ▼
PLATESOLVE 原始数据路径
  ├─ 内部唯一调用 sdet_detect_ex
  ├─ 保持原有选星、匹配、RANSAC、refine、WCS/SIP 路径
  └─ 在同一次调用中导出其内部检测得到的完整星表
        │
        ▼
star_det v1 ───────────► PSF(float32)
```

该路径不改变 PlateSolve 的输入和主要算法数据流，只增加一个只读导出点。若路径 A 在任一 TestData 上退化，当前版本必须自动选择路径 B，而不是阻塞工程，也不能强行放宽门限。

## 4. 决策规则

生产路径由 `P02-003` 全量 A/B 回归决定：

- **仅当全部登记 TestData 均通过非退化门限时，选择路径 A。**
- **任何一个旧路径成功样本在路径 A 下失败、超时、崩溃或质量退化，选择路径 B。**
- 不允许删除失败样本、临时修改检测参数、只展示平均值或把失败样本归为“异常数据”。
- 路径选择必须写入 ADR、构建能力信息、CLI 运行结果和 HISS provenance。

详细测试集冻结、重复性基线和判定方法见 `docs/18_PLATESOLVE_FULL_TESTDATA_DECISION_SPEC.md`。

## 5. `star_det` 数据块

统一格式为 FLOAT64 `[N,6]`：

```text
0 x_px
1 y_px
2 flux
3 mag
4 saturated       0/1
5 has_saturated   0/1
```

- 路径 A 的生产者是独立 `STAR_DETECT`。
- 路径 B 的生产者是 `PLATESOLVE_INTERNAL_EXPORT`。
- 一个生产运行中只能启用一个生产者。
- PSF 必须消费生产路径产生的同一块，不得再次调用 detector。
- 迁移期读取器可兼容旧 FLOAT32 `[N,4]`，新生产者只写 v1。

## 6. 路径 A 的候选 API

候选 API `ipv_solve_from_detections_v1` 直接接收 detections、图像尺寸、初始指向、焦距和像元参数，继续执行原有选星、Gaia 查询、投影、匹配、RANSAC 与 WCS/SIP 求解。

该 API 仅用于实验和 A/B，直到 G2 决策明确选择路径 A。未通过全量 TestData 前，不得替换正式 CLI 路径。

## 7. 路径 B 的检测结果导出 API

在不改变 PlateSolve 原始求解数据流的前提下，为原调用增加检测结果 sink/callback：

- callback 接收 PlateSolve 内部本次 `sdet_detect_ex` 得到的完整检测结果；
- callback 中 Orchestrator 立即复制为 `star_det v1`；
- callback 返回后源指针失效，不能跨 DLL 保存；
- 不允许为导出结果再次运行 detector；
- 旧 `ipv_solve_from_memory` 可继续作为无 callback 的兼容入口；
- 原有选星顺序、Gaia 查询、匹配、RANSAC、refine 和 WCS/SIP 逻辑保持不变。

优先采用调用期间 callback，而不是跨 DLL 返回需由另一模块释放的堆指针，以避免 CRT/allocator 所有权问题。

## 8. PSF float32 API

新增 `dpsf_fit_batch_f32`，直接接收 `const float* image`。内部拟合使用 float/double，不做 0–65535 clip。旧 uint16 API保留兼容测试。

不论最终选择路径 A 或 B，PSF 都必须：

- 消费同一个 `star_det v1`；
- 不调用 `sdet_detect_ex`；
- 不创建整张 uint16 图像；
- 记录消费的 block hash、count 和 schema。

## 9. Gaia 查询边界

- Astrometry query：PlateSolve 用位置和星等做几何匹配；
- Photometry query：测光模块需要 DR3SP 光谱、滤光片和 QE 积分。

两者语义不同，不强行共用三列 `gaia_cat`。处理方式：

1. 删除 Orchestrator 求解完成后的无消费者二次 `gaia_cat` 查询；
2. PlateSolve 可输出 `astrometry_matches` 供诊断；
3. GaiaClient 增加进程内只读缓存；
4. 后续可凭 source_id 缩小 Photometric 光谱查询，但不阻塞当前正确性。

## 10. 实施顺序

1. 冻结全部 PlateSolve TestData 清单、文件哈希、参数和旧路径结果；
2. 记录旧路径 detector 调用次数及 PlateSolve 重复运行抖动；
3. 定义 `star_det v1`；
4. 实现路径 A 作为实验候选，不切生产链；
5. 对全量 TestData 运行旧路径/路径 A A/B；
6. 自动形成 PASS/REJECT 决策：
   - PASS：生产链采用路径 A；
   - REJECT：实现路径 B，保留原 PlateSolve 数据路径并导出内部 detections；
7. PSF 切换 float32 API并消费选定路径产生的 `star_det`；
8. 删除第二次 detector 调用和无消费者 `gaia_cat` 查询；
9. 运行全量 PlateSolve、PSF 和 Stage 1 回归；
10. 记录最终路径、性能、内存、兼容性与回滚证据。

## 11. G2 验收

- PlateSolve 全量 TestData 清单已冻结且无事后排除；
- 路径 A 已完整 A/B，不以少量样本代替全量；
- 路径选择符合决策规则并有 ADR；
- 选择路径 A 时，全部样本均无退化；
- 选择路径 B 时，原 PlateSolve 主数据路径和结果保持基线一致；
- 每帧 `sdet_detect_ex` 恰好调用一次；
- PSF 与 PlateSolve 使用同一 `star_det` hash、N 和顺序；
- PSF 不再创建全图 uint16 缓冲；
- 生产 CLI 不允许在两条路径间静默回退；
- Stage 1 HISS 数值、元数据和 provenance 验证通过。

## 12. 回滚

- 路径 A 若在后续新增 TestData 中出现退化，可通过版本化配置回到路径 B；
- 回滚只能切换完整路径，不能形成“上游检测一次 + PlateSolve 内部再检测”的混合路径；
- 路径 B 是长期支持的保守兼容路径，不应被标记为临时失败方案。
