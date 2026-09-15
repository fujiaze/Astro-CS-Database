# W1 片2 · G_GOV_GATE｜P1 两条（HEAD a3a343a4）

### W1-N-05（P1）**新跨 DLL 边界结构不带自描述**：`hp_drizzle_api.h:104-111` 的 `HpAutoNsideResult`（P17 新增、`HP_DRIZZLE_API` 导出面，含 int 与 double×3 与 char[512]）**无 struct_size/abi_version 头**
- **对照尤其刺眼**：同区间 P18 刚按宪章 §8.6 给 `IpvParams` 补齐**四件套**（首部双 uint32 + 入口 fail-closed + 布局锁 + 自检），而 `V11-N-10` 定稿结论就是「**唯一稳定解是给边界结构补 struct_size 让运行期自拒，勿维护镜像清单**」并点名要扩散。⇒ **新增边界结构未继承同区间刚立的纪律**（簇 6「新边界不继承老边界纪律」的 ABI 版）。
- **后果**：DLL 新旧错配时**无运行期自拒**——调用方 memset 后按错误布局读 `result->nside` 等**静默取到错值**（这正是 `V2-N-01` 那类 72 字节错位的复现路径）。related 机制⑪、`V11-N-01..10`、`V2-N-01`、`C-14`、§8.6

### W1-N-06（P1）「负责人裁定」**零登记**（机制⑫ 新实例）：P17 nside 语义三处引用一条不入登记面的裁决
- 引用站点：`hp_drizzle_api.h:77-81`、`module_adapters.cpp` 的 `p1_op_drizzle` P17 块注释、`p1001_real_nodes_test.cpp:2334`，均称负责人裁定（采样率等价 drizzle 1x-2x）。
- **登记面三级复核全零**：检索 `1x_to_2x`、`1x-2x`、采样率等价于 `CHANGELOG.md`、`memory.md`、`问题扫描/40_OWNER_DECISIONS.md`、`工程控制/` ⇒ **全部 0 命中**。⇒ 按 `E10` 判据，**进不了登记面的裁决不可证伪**：无法核对措辞、无法判是否被后续裁决覆盖、也无法据此判定测试期望是否权威。
- 同批 `PSF-FAST-001` 族已由 `V8-N-07`/`A-44` 挂账 ⇒ **本条是同形态新站**，不另立机制。处置：请负责人把该裁决补进 `40_OWNER_DECISIONS.md`（新增 A 号）或明确撤销；在其登记前，`p1001` 里据它钉死的期望应标为**待裁**。related `V8-N-07`、`A-44`、机制⑫、`E10`
