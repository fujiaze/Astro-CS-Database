# HISS 容器与 Tile 规范

> 本页面为已冻结规范，源自 `02_FROZEN_STAGE1_HISS_SPEC.md`。Agent 可实现和文档化，不得自行改写科学语义。

## 1. 自适应空间 Tile

每个 Tile 对应一个 NESTED 父像素。层级差：

\[
d=\min(9,\log_2(NSIDE/16))
\]

\[
NSIDE_{tile}=NSIDE/2^d
\]

同时保证：

- 满 Tile 最多 `4^9=262144` 个叶像素；
- Tile 父级 NSIDE 不低于 16；
- Tile 特征角尺度不超过约 3.7°。

## 2. Tile 占用编码

Writer 支持：

- `FULL`：全部叶像素有效，无占用块；
- `BITMAP`：1 bit/潜在叶像素；
- `SPARSE_LIST`：保存有效局部索引。

用户和 GUI 不配置模式。**具体切换阈值尚未冻结，必须用 C++ 实验后汇报。**

## 3. 独立子块

每个 Tile 至少包含独立可寻址子块：

- occupancy（FULL 时省略）；
- signal；
- support；
- 可选 SNR controls；
- 未来可选扩展。

不得依赖固定物理顺序解释。未知可选子块可跳过；未知必需子块必须拒绝。

## 4. HISS 容器

参考 XISF 的单体容器思想：

```text
固定签名块
→ 文件前部完整 Header
→ 独立 attachment 子块
```

- Header 是唯一权威目录；
- **不使用 Footer、Checkpoint、断点续写**；
- 写入先流式生成临时子块池，再生成最终 Header，组装 `.partial`，flush 后原子重命名；
- Header 使用紧凑可扩展二进制结构，不照搬 XML；
- 当前只维护一个 AstroCS 1.0 目标 HISS 格式。

## 5. 子块目录必需字段

每个子块目录项必须独立记录：

- block type；
- required/optional flags；
- offset；
- compressed size；
- uncompressed size；
- `codec_id`；
- `transform_id`；
- checksum type；
- checksum。

同一 HISS 中允许不同 Tile、不同子块使用不同 codec/transform。必须支持 RAW。文件级默认 codec 只能用于显示或建议，不能替代子块级声明。

## 6. 实现要求

### Writer

- 支持自适应 Tile 规则；
- 支持FULL/BITMAP/SPARSE结构，但切换阈值不得最终定案；
- 每 Tile 独立子块；
- Header 前置、attachments 后置；
- 使用临时子块池和最终 `.partial` 原子提交；
- **不实现 Checkpoint、Footer 和断点恢复**；
- RAW 必须可用，其他 codec 通过注册接口接入；
- 未冻结的默认 codec 不得硬编码为正式规范。

### Reader

- 按目录读取，不依赖子块物理顺序；
- 支持只读 occupancy、signal、support 或 SNR；
- 未知可选块跳过；未知必需块报不兼容；
- 检查 offset/size 越界、解压长度和 checksum；
- 用 NSIDE/NESTED/ICRS 定位，**不依赖 WCS**。
