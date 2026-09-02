# IO-001 FITS 流式接口冻结合同

> doc_id: DOC-IO-INTERFACE-001
> doc_status: ACTIVE_NORMATIVE
> task_id: IO-001 · wave: W2 · owner: SA-IO-07
> commit: `feat(io): IO-001 建立FITS流式接口`（前台集成）
> source: `tasks/03_RUNTIME_DATA_IO_TASKS.md` IO-001 / `05_FIXED_SUBAGENT_BINDINGS.yaml` SA-IO-07 / 冻结约束 `AstroCS_ENGINEERING_CONSTRAINTS.md` F.3（DLL C ABI 边界）

## 1. 目标与范围

IO-001 是 **FITS 流式 I/O 接口冻结 + 骨架实现**（W2 宿主基础设施，非 W3 科学迁移）：

1. 把 FITS 第三方库（CFITSIO 等）隔离在 `astrocs_io.dll` **内部**；公开边界是纯 C ABI，
   **CFITSIO 类型/句柄（`fitsfile*` 等）绝不跨 DLL 边界**。
2. 提供 header / plane / chunk 三级读接口、原子写接口、checksum(DATASUM/CHECKSUM)/verify、稳定错误码。
3. 可观测性：每次 read/write 的 **bytes 累计到 trace 计数器**（宿主注入），供运行时 I/O 监控使用。
4. 验收负测覆盖：非法 header、截断、dtype/shape/unit mismatch、checksum error、NaN/Inf、取消、磁盘满。

本任务**不改科学公式**（`scientific_change=false`），不迁移任何科学/图像处理算法；
`lib/astro_image_io`（aio_fits 等历史全图像读写实现）与 `lib/io`（io_adapter）**保持原样、不删除**。

## 2. 模块归属与目录

| 内容 | 路径 |
| --- | --- |
| 本冻结合同 | `docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md` |
| io 服务模块骨架（README/module.yaml/CMake/占位入口） | `modules/services/io/` |
| FITS 流核心（C 实现、私有，DLL 内） | `runtime/io/fits_core.c` |
| FITS 流 C ABI（只被 astrocs_io.dll 导出） | `modules/services/io/include/astrocs/io/fits_stream_v1.h` |
| 模块公开 ABI 占位（module query 入口） | `modules/services/io/include/astrocs/io/io_module_api_v1.h` |
| 契约/负测（Python，依赖 numpy/astropy 作 oracle） | `tests/io/` |
| C 层自检驱动 | `modules/services/io/tests/` |

允许写路径：`runtime/io/** lib/io/** lib/astro_image_io/** lib/healpix_db/** modules/services/io/** tests/io/** docs/interfaces/io/**`。

## 3. DLL 边界与所有权

- 目标 DLL：`astrocs_io.dll`（Windows）/ `libastrocs_io.so`（Linux 技术预览）；本任务骨架。
- **第三方隔离**：CFITSIO 只在 `astrocs_io.dll` 内部编译；fits_core 不包含任何 CFITSIO 头，
  不出现 `fitsfile` 等类型；若未来以 CFITSIO 实现读写，均封装在 DLL 内私有层。
- 跨边界类型：POD 结构 + 固定宽度整数 + 定长 UTF-8 字符数组 + opaque handle + 回调；禁 STL/异常/RTTI。
- 所有跨边界结构体前两字段为 `struct_size` / `abi_version`（同 `acs_head` 模式）。
- 内存所有权逐函数标注；本骨架只读路径无跨边界分配（out 缓冲由调用方分配）；错误文本由调用方缓冲。
- 版本：module/ABI/data-schema/product 版本分离；本接口 ABI 版本固定 `1`，扩展必须升版本。
- 文本公共格式 UTF-8；Windows 内部路径 UTF-16 属上层适配层，不进入本头。

## 4. 错误码（v1 冻结，与 common_abi_v1 一致数值）

| 码 | 名称 | 语义 |
| --- | --- | --- |
| 0 | `ACS_FIO_OK` | 成功 |
| 1 | `ACS_FIO_ERR_PARAM` | 参数非法（NULL/越界/非法 dtype/shape/unit 值） |
| 2 | `ACS_FIO_ERR_ABI_MISMATCH` | ABI 版本/结构大小失配 |
| 3 | `ACS_FIO_ERR_NOMEM` | 内存不足 |
| 4 | `ACS_FIO_ERR_IO` | 底层 I/O 失败（含磁盘满等 errno 映射） |
| 5 | `ACS_FIO_ERR_UNSUPPORTED` | 不支持（BITPIX/NAXIS/压缩等超出骨架支持域） |
| 6 | `ACS_FIO_ERR_CANCELLED` | 用户/宿主取消 |
| 7 | `ACS_FIO_ERR_STATE` | 句柄状态错误（未打开/已关闭/顺序错误） |
| 8 | `ACS_FIO_ERR_TRUNCATED` | 文件截断（数据区不足） |
| 9 | `ACS_FIO_ERR_BAD_HEADER` | FITS header 非法/END 缺失/卡片语法/保留卡冲突 |
| 10 | `ACS_FIO_ERR_MISMATCH` | dtype/shape/unit 与声明不符 |
| 11 | `ACS_FIO_ERR_CHECKSUM` | DATASUM/CHECKSUM 校验失败 |
| 12 | `ACS_FIO_ERR_NANINF` | 数据含 NaN/Inf 而策略要求拒绝 |
| 13 | `ACS_FIO_ERR_DISKFULL` | 磁盘满（ENOSPC/EDQUOT 映射） |

错误文本优先：调用方提供 `char err[96]`（可为 NULL）；失败码之外的诊断应进宿主 logger/trace。

## 5. FITS 支持域（v1 骨架）

| 项 | 值 |
| --- | --- |
| FITS 标准 | FITS 4.0 基本图像 HDU（NAXIS≥0，≤3） |
| BITPIX | 8 / 16 / 32 / 64 / -32 / -64 |
| 扩展 | 拒绝（`XTENSION` 出现即 `ACS_FIO_ERR_UNSUPPORTED`，v1 图像域） |
| 字节序 | big-endian 存储；本机小端自动转换（读入/写出均按 dtype 元素交换） |
| BSCALE/BZERO | header 读取原样呈现（关键字表）；**v1 不自动应用缩放**——调用方按 BZERO/BSCALE 语义显式处理（明确避免隐式标度误差） |
| NaN/Inf | 读取时默认放行（FITS NaN 约定以 IEEE 位模式存在）；验证平面时若策略=拒绝且存在 NaN/Inf → `ACS_FIO_ERR_NANINF` |
| 随机群/表/压缩 | 拒绝（`ACS_FIO_ERR_UNSUPPORTED`） |

## 6. 关键字契约（v1）

- 关键字表为 80 字节卡片顺序视图；卡片 ≤ `ACS_FIO_MAX_CARDS`（1024），超限拒绝。
- 读取时 END 卡片之前的所有有效卡片入表；`name`（≤9 字符）、`value`（≤72，原样去首尾空白、去引号）、`comment`。
- 标准形如 `NAXISn`/`BITPIX`/`BUNIT` 等：v1 **只信任读取时解析出的结构值**；
  调用方以 `fits_get_header` 的结构字段（bitpix/naxis/shape）为准。
- `COMMENT`/`HISTORY`/`END`/空卡片：写路径自动生成；读路径跳过（`END` 后卡片非法）。
- 保留关键字（写路径校验）：`SIMPLE`、`XTENSION`、`BITPIX`、`NAXIS`、`NAXISn`、`PCOUNT`、
  `GCOUNT`、`EXTEND`、`BSCALE`、`BZERO`、`BLANK`、`DATASUM`、`CHECKSUM` —— 由 writer 内建，
  不允许调用方自定义卡片占用；占用返回 `ACS_FIO_ERR_BAD_HEADER`。
- 写路径卡片总数（含内建+校验所需）超 1023 拒绝。
- `BUNIT`：写路径从调用方 `bunit` 字段写；读路径在 header 中保留，verify 按 dtype/shape/unit 三项分别校验。

## 7. 句柄与并发

| 接口 | 并发合同 |
| --- | --- |
| `fits_reader_open` | reentrant；返回句柄后与其它调用无共享状态 |
| `fits_reader_*` 系列 | 同一句柄串行（句柄非共享）；不同句柄可并行 |
| `fits_writer_*` 系列 | 同上 |
| `fits_verify_file`/`fits_compute_file_datadigest` | reentrant；调用期间内部文件只读；无全局状态 |
| `fits_core_trace_*` | reentrant；tls/本地累计，读取函数返回后可见 |

取消：宿主注入 `is_cancelled` 回调（可 NULL）；在 header 解析块、chunk 边界检查；
置位则返回 `ACS_FIO_ERR_CANCELLED`，读路径句柄保持可继续使用或由调用方关闭。

## 8. trace / bytes 记录

宿主注入：

```c
typedef struct acs_fio_trace_hooks_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    /* read_bytes/write_bytes: 每次底层文件 read/write 完成后以实际字节数回调一次 */
    void (*on_read_bytes)(void* ud, uint64_t bytes);
    void (*on_write_bytes)(void* ud, uint64_t bytes);
    int  (*is_cancelled)(void* ud);          /* 0/1 */
    void* user_data;
} acs_fio_trace_hooks_v1;
```

- read_bytes 计数：底层字节读取（header 块 + 数据区），与调用方请求的 plane/chunk 元素数换算无关。
- write_bytes 计数：底层字节写入（含 2880 块填充与校验卡重写）。
- 计数语义：每次 read/write 调用后 hook 被触发；hook 不保证在调用返回前可见于 trace 记录——
  由宿主在 trace 落点时取累计值（IO-001 验收：read/write bytes 记录到 trace 由运行时侧 IO-00x 接线）。

## 9. 校验与 checksum

- `fits_compute_file_datadigest`：**独立实现**的 FITS DATASUM 算法（32 位 1 的补码块校验，
  按 2880 字节块、16-bit 大端字累加、进位回卷），返回 10 位十进制字符串；算法独立（不链接 CFITSIO）。
- `fits_verify_file` 校验顺序：
  1. header 结构合法性（非法 header 拒绝）；
  2. 数据区长度 ≥ 声明的 NAXIS 乘积×字节宽（截断拒绝）；
  3. 若卡片含 `DATASUM`：对数据区（含数据填充到 2880 块）重算并比较（checksum error 拒绝）；
  4. 若卡片含 `CHECKSUM`：CHECKSUM 通过把 CHECKSUM 卡片本身置 0x30('0')×16 后对整个 HDU 校验（标准算法）。
  5. `strict_nan`=1 时，调用方提供的样本平面做 NaN/Inf 检查；文件校验本身不隐式全扫 NaN（数据平面
     NaN 是合法 FITS 表示）。
- DATASUM 全零数据区与"无 DATASUM 卡"（默认 0）等价：**必须写 DATASUM 卡**，verify 才做数据校验。
- 写路径默认在最终产物写 DATASUM 卡；CHECKSUM 卡由调用方选项决定（默认关闭，避免与 fitsverify 无关语义混叠）。

## 10. 原子写

- 目标路径同目录临时文件 `.<name>.tmp.<pid>.<seq>` → 全量写入 → 重读校验（可选项）→
  `rename`（POSIX 原子）→ 完成。
- 写失败/取消/校验失败：删除临时文件，目标文件保持原状；返回对应错误码。
- 默认不覆盖已存在目标（`ACS_FIO_ERR_IO`，EACCES 语义）；`overwrite`=1 时允许替换。
- 磁盘满（ENOSPC）映射 `ACS_FIO_ERR_DISKFULL`。

## 11. 使用序列（骨架）

```text
读:
  acs_fio_trace_hooks_v1 hooks = ACS_FIO_TRACE_HOOKS(ud, on_read, on_write, is_cancel);
  int st = fits_reader_open_v1(path, &hooks, &rd, err);
  fits_header_v1 hdr; st = fits_get_header_v1(rd, &hdr, err);   // 结构 + 关键字
  // 平面读（整幅）:
  st = fits_read_plane_v1(rd, 0, plane_nx, plane_ny, expected_bpix, bunit,
                          buf, elem_cap, &got, trace, err);
  // 或 chunk 读:
  st = fits_read_chunk_v1(rd, plane, first_elem, count, buf, &got, trace, err);
  fits_reader_close_v1(rd);
写:
  fits_writer_begin_v1(path, &hdr_struct, bunit, &trace, &wr, err);
  fits_write_plane_v1(wr, plane_index, nx, ny, bpix, data_bytes, &trace, err);
  // 或 fits_write_chunk_v1(wr, elem_count, data_bytes, &trace, err);
  fits_writer_end_v1(wr, 1 /*write_datasum*/, 0 /*write_checksum*/, &trace, err);
  // 失败路径: fits_writer_abort_v1(wr);
校验:
  st = fits_verify_file_v1(path, 1 /*verify_checksum*/, err);       // 负测: 篡改数据后返回 ACS_FIO_ERR_CHECKSUM
  st = fits_compute_file_datadigest_v1(path, buf10, &len, err);
```

## 12. 契约/负测验收映射（tests/io/）

| 验收 | 测试 |
| --- | --- |
| 非法 header | `fits_bad_header_*`（SIMPLE 缺失/BITPIX 非法/无 END/卡片越界）→ `ACS_FIO_ERR_BAD_HEADER` |
| 截断 | `fits_truncated_*`（截断到 header 中/数据区一半/2880 边界内）→ `ACS_FIO_ERR_TRUNCATED` |
| dtype/shape/unit mismatch | `fits_mismatch_*`（-32 文件按 -64 读 / NAXIS 不符 / BUNIT 不符）→ `ACS_FIO_ERR_MISMATCH` |
| checksum error | 写 DATASUM 后篡改一个字节 → `fits_verify_file` = `ACS_FIO_ERR_CHECKSUM`（astropy 交叉验证） |
| NaN/Inf | 合成含 NaN/Inf 平面，strict 策略 → `ACS_FIO_ERR_NANINF`；默认读策略放行 |
| 取消 | 注入置位 cancel 回调 → `ACS_FIO_ERR_CANCELLED` |
| 磁盘满 | 目录权限拒绝（EACCES 写失败）→ `ACS_FIO_ERR_IO`/`ACS_FIO_ERR_DISKFULL` 映射负测 |
| read/write bytes 入 trace | hook 计数累计与文件实际长度一致断言 |
| astropy 交叉 oracle | 自产 FITS ↔ astropy 读；astropy 产 FITS（含 DATASUM）→ 本实现 verify |

## 13. 与相邻接口/实现的边界

- `lib/astro_image_io`（AIO，含 CFITSIO 静态链）：历史全图像读写（aio_read/write_fits），
  保留作兼容层；**IO-001 不迁移/不修改/不删除**，其内部 CFITSIO 用法同样不跨 DLL 边界。
- `lib/io` + `include/astrocs/io/io_adapter.h`：Artifact 事务 + FileIoAdapter（历史 IO-001 原型），保留。
- `runtime/io/fits_core.c` 是本任务新增的 fits 流 C 核心（无 CFITSIO 依赖）。
- DATA-001 `astrocs/contracts/artifact_abi_v1.h`：产物 manifest C ABI；fits 流接口不重复其职责。
- trace/bytes：由宿主注入 hook（14 标准）；本任务只冻结 hook 契约并累计，运行时落点由后续 RT 任务接线。
- HiPS/manifest 输入输出（IO-002/IO-003）在本接口之上扩展，本任务不实现。

## 14. 已知限制（v1 骨架）

1. Linux 控制节点（本任务执行环境）无 MSVC/Windows DLL 构建；产出 C ABI + 模块骨架 +
   Linux `.so` 技术预览与全部契约/负测。Windows 正式 DLL 构建（astrocs_io.dll）在 W6 用同一源码执行。
2. 只支持基本图像 HDU（NAXIS 0–3）；表/随机群/压缩扩展 → `UNSUPPORTED`（后续任务扩展）。
3. 不自动应用 BSCALE/BZERO（避免隐式标度）；调用方显式处理。
4. CHECKSUM 卡写路径默认关闭；verify 支持标准 HDU CHECKSUM 校验。
5. 磁盘满负测用目录权限(EACCES)近似 + ENOSPC 码路径静态覆盖（2c2g 无法真实填满磁盘）。
6. 取消回调为宿主注入；无宿主时测试注入桩验证语义。
