# 插件文档：aio（I/O 与原子提交）

## 1. 职责与边界

- **职责**：FITS/HiPS/manifest 的**唯一 I/O 边界**：读、写、校验、原子提交、缓存。
- **不是**：不做科学计算；不做重采样/投影；Phase1/2/3 复用同一套 AIO，禁止各自复制 reader/writer。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §7.2（架构）、§9（I/O 与原子产品）
- FITS Standard、IVOA HiPS 1.0

## 3. 输入/输出数据合同

- **输入**：FITS/HiPS/manifest 路径、数据对象（signal/variance/coverage/...）、配置。
- **输出**：原子提交的产品目录/文件 + manifest + 校验记录。
- 参考：`contracts/schemas/*.schema.json`（全部数据产品）。

## 4. 算法与公式要点

- 所有产品：临时文件/目录 + 校验 + fsync + 原子 rename 提交；
- 失败/取消不得留下可被误认为正式产品的半成品；
- 读：格式校验、哈希校验、单位/形状/所有权检查；
- 写：分层写（先数据后 manifest）、flush/close/fsync、checksum、原子 rename；
- 缓存：只缓存不改变科学值；容量有界、可失效。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `cache_mb` | —— | MB | 缓存上限 |
| `fsync` | true | —— | 原子提交 fsync |
| `checksum` | true | —— | DATASUM/CHECKSUM |

## 6. 接口/ABI

- 公共读写 API（C ABI 版本化），供 lib/infrastructure/cli/scheduler/科学模块调用；
- 是唯一允许触碰磁盘产品的模块。

## 7. 错误与边界

- 哈希/格式不匹配 → 拒绝读取；
- 缺 tile/非有限 → validity 标记，不以零填充；
- 取消 → 无半成品；
- I/O 错误 → exit 7。

## 8. 测试与 Oracle

- 原子提交测试：中途失败/取消无半成品；
- 重开独立验证与写入一致；
- 哈希/校验失败拒绝读取；
- 缓存正确性（不改变科学值）；
- 长路径、>2 GiB、Windows/Linux 双平台。
