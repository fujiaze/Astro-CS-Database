# 运行清单与验证

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
