"""Preserve CLI/manifest sources and describe actual public behavior."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/engineering/contracts/MANIFEST_VERIFY.md': r'''# 运行清单与验证

运行清单记录一次命令执行的输入、软件、配置、产物和结果。它不同于阶段科学配置、机器画像、逐帧科学sidecar及产品完成清单。配置形状见[配置合同](CONFIG.md)，内容发布见[原子发布](ATOMIC_PUBLISH.md)，来源语义见[溯源](../data/PROVENANCE.md)。

## 1. 生产写出

`lib/infrastructure/cli/commands.cpp::write_run_manifest`写入块级`<output_dir>/acsd_run_<run_id>.json`，先tmp、flush和大小核对，再rename，登记artifact事件和final所引用的清单路径。该路径有实际写入/rename检查，不等于已经具备完整文件/目录fsync持久化确认。

| 字段 | 当前来源与解释 |
|---|---|
| `schema_version/kind` | `1` / `acsd_run_manifest`，当前生产运行清单身份 |
| `run_id` | 事件发射器的本运行身份 |
| `acsd_version` / `platform` | 二进制构建版本与编译平台 |
| `config_path/config_sha256` | 实际输入配置路径及文件字节摘要 |
| `cpu_profile_path/cpu_profile_sha256` | 当前writer基础对象写null；不能宣称该面已记录并验证所用画像 |
| `phases` | 本独立命令阶段编号列表，不表示自动串接 |
| `artifacts[]` | role/path/sha256/size_bytes，实际收集对象 |
| `status` | complete或incomplete；非complete时带error.message |
| `started_utc/finished_utc` | 当前函数在写清单时分别取now，尚不能当实际运行起止历时 |
| `summary`及extra | 运行说明和调用者汇总的节点事实；缺失不补假值 |

字段表描述当前writer，不定义新的科学schema。`run_manifest.schema.json`仍有与此不同的词表，不能据本表断言现有产物已通过它。目标provenance包括算法、构建、provider、worker、输入/产物摘要、单位/坐标及能力；具体字段须沿extra实际装配核对。`log_artifacts[]`完整双日志登记仍见[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 2. 产品清单和存储事实

逐帧signal/模型/sidecar有自己的完成状态；运行complete不自动表示所有科学误差模型已具备。不同清单的COMPLETE/complete大小写、schema和根路径按各自协议解释，不能凭相似键互读。

运行级storage字段的目标定义见[存储形态](HIPS_STORAGE_FORM.md)，内容tree_hash、容器sha256和索引sha256分别具名。归档容器不能把自身哈希写进被封装内容形成自引用；产品级清单和运行级事实分工不因此证明接线已完成。

构建source_sha、build_head_sha、build_source_digest、dirty与实际二进制哈希的证据范围见[发行合同](../build/RELEASE.md)。只有实际采集的值可写入；配置值不能冒充观测worker/provider。

## 3. 实际验证命令

```sh
acsd doctor --json --run-manifest <run-manifest.json>
```

该机器旗标进入`commands.cpp::cmd_verify`，不是独立verify命令。当前顺序与返回为：

1. 文件/JSON及kind/schema_version失败→3。
2. status非complete→8。
3. acsd_version与本二进制字符串不同→5；这是当前校验器限制，不是旧产品科学上必定不可读。
4. phase3须有phase3_output角色，phase1/2要求非空artifacts，否则→8。
5. 配置路径给出时重算配置字节hash，不可读/不符→3。
6. 各artifact存在性→3，整文件SHA-256和大小不符→8。
7. 通过则输出一个JSON文档`{verify:"ok",checked:N,manifest:...}`并返回0。

当前代码没有重算cpu_profile hash的分支，也未显示执行完整run_manifest schema、逐帧模型或所有科学恒等式验证。checked是上述检查项计数，不是科学测试数量。畸形字段类型、目录型artifact、相对路径解析与缺失嵌套字段还需负例核对，不能把“验证ok”提升为完整科学验收。

## 4. 配置与画像分离

科学配置由parser/precheck和所属模块解释；cpu_profile是benchmark产出的机器绑定文件，缺失/失配可按明确基线策略处理。schema兼容分支和独立校验工具存在，不证明CLI已经消费全部字段。禁止把科学配置、画像和运行事实合并成一个模糊config对象。

## 5. 错误与验证记录

清单写失败是I/O故障，已写产品完整性失败是另一语义，映射见[日志错误](LOG_AND_ERROR.md)。取消或节点失败尝试写incomplete及原始原因；没有合法输出目录或磁盘故障时如实报告清单未写成。

检查配置篡改、产物缺失、错误大小/摘要、畸形字段、部分写、取消、版本差异与来源未接线。旧文列出的test_01..等名称不作为现有执行证据，完整原文保存在[来源档案](../../../实验/documentation-refactor/docs/CLI_RECORDS_SOURCE.txt)。本轮未运行doctor或科学命令。
''',
 'docs/detail/infrastructure/cli.md': r'''# CLI 实现

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
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/CLI_RECORDS_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'CLI source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
