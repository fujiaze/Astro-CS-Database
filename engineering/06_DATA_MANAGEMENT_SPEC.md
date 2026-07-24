# 06 数据管理与数据契约 Spec

## 1. 管理对象

1. 原始观测 FITS/XISF；
2. Bias/Dark/Flat 与主帧；
3. Gaia DR3/DR3SP 数据；
4. PipelineFrame 命名块；
5. `.aio` 调试缓存；
6. `.hiss` 单帧产品；
7. `.hcsd` 数据库；
8. 合成测试数据与黄金结果；
9. 日志、指标、检查点和运行清单。

## 2. 数据集注册

所有用于测试或验收的数据必须登记到 `control/DATASET_REGISTER.csv`，并有 Dataset Card。至少记录：

- dataset_id / version；
- 来源与许可；
- 文件列表或生成器版本；
- SHA-256；
- 图像尺寸、位深、滤镜、曝光、WCS、FOV；
- 预期用途；
- 预期结果与容差；
- 是否包含敏感/不可公开内容；
- 本地存放规则。

## 3. 原始数据不可变

原始 FITS、主帧和黄金数据只读。所有处理输出写入 run-specific 目录：

```text
data/runs/<run_id>/
  run_manifest.json
  config.resolved.json
  inputs.sha256
  stage1/
  stage2/
  logs/
  metrics/
```

## 4. 运行清单

每次端到端运行必须生成 `run_manifest.json`：

- run_id 和 UTC 时间；
- Git commit、dirty 状态；
- DLL/EXE SHA-256；
- 配置原文与解析后配置；
- 输入文件哈希；
- 环境与线程数；
- 每阶段开始/结束、耗时、峰值内存、结果；
- 输出文件哈希；
- 警告、退化路径和回退路径。

## 5. PipelineFrame 契约

### 5.1 Schema 注册

建立机器可读 `pipeline_blocks.schema.json`。每个块定义：

- 名称；
- schema_version；
- 类型、维度、字段顺序；
- 单位；
- 坐标原点（0-based/1-based）；
- 生产者、消费者；
- 必需/可选；
- 生命周期；
- 空值/NaN 规则；
- 兼容策略。

### 5.2 当前必须复核的冲突

- `psf` 注释中曾出现 `[6]`、`[N,9]`；
- 标准块表仍提到稠密 `snr`，实际管线改为 `snr_model`；
- WCS CRPIX 在不同 API 中有 0-based/1-based 风险；
- `star_det` 的字段数与语义需以代码和调用链共同确认；
- `gaia_cat` 是否由 PHOTOMETRIC 复用，现状可能重新查询。

## 6. HISS 契约

冻结前至少明确：

- magic、格式版本、字节序；
- header 编码与压缩；
- nside、nested、ipix 类型；
- pixel 单位与是否已做全局测光 scale；
- `snr_format=0/1` 的兼容行为；
- 稀疏控制点精确二进制布局；
- 元数据必需字段；
- block offset、长度与校验；
- 截断/损坏文件的错误行为；
- 未来扩展字段如何跳过；
- 写入原子性。

HISS 必需元数据建议：

- format_version；
- source_file_hash；
- pipeline_commit；
- WCS/SIP 摘要；
- filter/QE/photometric scale；
- SNR model 参数；
- nside 选择策略与实际像素尺度；
- coverage/FOV；
- 处理退化标记。

## 7. HCSD 契约

除现有像素与子叶索引外，P02 必须决定是否保存：

- coverage count；
- accumulated weight；
- variance/uncertainty；
- per-leaf statistics；
- 输入 HISS 哈希列表；
- gradient/stack 参数；
- rejected sample statistics。

若暂不保存，也必须说明为什么，以及未来格式升级方案。

## 8. 数据契约测试

每个格式至少测试：

- 同版本 round-trip；
- 旧版本读取；
- 新增未知字段；
- 空数据、单像素、最大合法 nside；
- nside<64 与 nside≥8192；
- 截断文件、错误 offset、错误长度、错误 magic；
- 非法 NaN/Inf；
- 大文件偏移（>2GB 规划测试）；
- 跨编译器结构体布局；
- 多次写出字节级确定性（允许的时间戳字段除外）。

## 9. 数据保留

- 原始数据：永久或外部归档；
- 黄金数据：永久、版本化；
- 每次普通开发运行：保留摘要和失败证据；
- 发布候选完整证据：永久；
- 临时中间产物：任务完成后可清理，但先记录哈希和报告。
