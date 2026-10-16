# CLI 实现

CLI解析参数、配置、运行控制和事件，调用各独立阶段Runtime。协议正本为[命令行合同](../../engineering/contracts/CLI_PROTOCOL.md)，配置见[CONFIG](../../engineering/contracts/CONFIG.md)，错误映射见[日志错误](../../engineering/contracts/LOG_AND_ERROR.md)。科学计算在算法库，文件读写在AIO。

## 1. 真实调用

```text
CLI main / parser
  → command_tree.h 与 session_commands.h
  → subcommand.h 的配置预检与现有交互
  → commands.cpp::run_phase1_block / run_phase2_block / run_phase3_block
  → runtime_client.cpp::run_pipeline → Runtime → 节点adapter
  → AIO产品 + 运行清单 + 收尾事件
```

三个命令为normalize/mosaic/export，不隐式串接；局部session facade不等于完整命令。Runtime借用调用方RunContext，取消监视和在飞消费者结束后再释放，详见[Runtime细节](runtime.md)。

## 2. 命令与输出

command_tree.h提供help、--version、doctor、benchmark和三个科学命令；旧phase1/2/3与verify等用户别名不在命令表。`--json <config>`对科学命令是配置路径，运行stdout为默认JSONL事件；`--events-jsonl`是兼容别名。版本/doctor机器响应和模板输出是单个JSON文档，不能只凭--json字面判断输出形态。

模板由session_commands.h字段信息与生成器组成，`--template`及-o/--output选择落点。生成结构与真实合法输入分开：占位路径、示例数值不表示科学条件已经具备。GUI捕获事件流和退出码，不从普通stderr文本判断是否产生科学产品。

## 3. 配置与现有预检

parser.cpp::validate_config_full负责对象/键/类型、块与输入路径等检查，模块负责科学值域。blocks[]与平铺形态互斥，每块明确output_dir及输入。未知键、不可读输入、非法几何等具体报错；不同设备/滤镜名称不构成人为拒绝理由。

当前subcommand.h保留预检页面及confirm_run；-y/--yes跳过交互，EOF不当用户确认。-force是兼容路径，不能描述成越过所有检查：config_structure_errors等结构拒绝仍存在。该CLI行为不转化为文档研发的Agent审批或负责人发布许可。

benchmark画像路径由cli_resolve_cpu_profile_path决定，安装位置不可写/构建树环境的回落须报告实际路径。机器字段是否进入生产路由按实际消费者核对，不从profile文件存在推定已生效。

## 4. 事件、清单与错误

JsonlEmitter和protocol.h校验事件身份、序号、kind与扩展字段；人可读摘要走stderr。错误通过Result/节点manifest进入runtime_client的域映射；内存、CPU和线程利用率不触发产品资源超限退出，磁盘写失败按相应I/O/RESOURCE错误处理。

write_run_manifest写`acsd_run_<run_id>.json`并回填final路径；字段与doctor校验范围见[运行清单](../../engineering/contracts/MANIFEST_VERIFY.md)。正式结构化日志、Core logger和stdout事件是不同通道，不能声明现有CLI已经写出完整log_artifacts[]。仅开发快照失败与正式I/O失败分开处理。

取消在内核安全点传播，停止派发、等待在飞借用、收尾并记录incomplete。已经完成的独立帧与已可见但持久化未确认对象按各自发布合同处理，不能笼统删除所有结果。崩溃报告去敏并保留最初错误上下文。

## 5. 验证范围

核对命令表、帮助/模板、合法配置到真实节点、未知键/退役形态、预检交互/非交互、JSONL序号/终止事件、取消、逐域退出码、日志缺失与清单验证。双平台实测、静态调用和独立科学评价分别报告；旧测试名与历史协议描述存入[来源档案](../../../实验/documentation-refactor/docs/CLI_RECORDS_SOURCE.txt)，不声称本轮执行通过。
