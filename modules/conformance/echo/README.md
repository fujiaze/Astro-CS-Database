# echo — conformance 跨平台契约探针模块 (ABI-005)

> 状态：`IMPLEMENTED`（ABI-005 conformance; 无科学含义）。本模块与 noop
> （BLD-003 SKELETON 可加载性骨架）配套：noop 只验证“能加载、能握手”，
> echo 验证模块对 **host 注入的全部 service callback 的正/负语义** ——
> artifact read/manifest、host allocator、logger、cancel、executor lease、
> config query、错误域；artifact **write** 在 ABI v1 冻结面不存在写回调，
> 模块如实返回 `ACS_ERR_UNSUPPORTED`，绝不绕过 host 自开文件（FORBID-002）。

## 1. 标识与状态

| 项 | 值 |
|---|---|
| 模块 ID | `astrocs.conformance.echo`（MOD-ECHO） |
| DLL target | `astrocs_echo`（Windows `astrocs_echo.dll`；Linux `astrocs_echo.so`） |
| module/ABI/doc revision | module 0.11.0-alpha.1 / ABI v1 / ABI-005 |
| owner | SA-ABI-03（ABI-005） |
| 状态 | `IMPLEMENTED` — ABI-005 conformance；无科学含义 |

## 2. 负责范围 / 不负责

负责：
- 唯一导出入口 `astrocs_module_query_v1`（12 §1：module DLL 不得导出其他
  符号）—— `nm -D` 实测仅该符号（ABI-006 全查 exports）；
- 每个 host service callback 的正/负探针（action 白名单）：
  - `echo`：回显 input manifest（内容往返一致；含 cancel 前置检查点）；
  - `artifact_read`：host.artifacts 全链（open/query/read_all/manifest
    parse/get_str/get_u64/close）；host 服务缺失 → detail 101；
  - `write_blocked`：请求写通道 → `ACS_ERR_UNSUPPORTED`（detail 105；
    ABI v1 无写回调，不自开文件 FORBID-002）+ host logger WARN 正测；
  - `cancel_poll`：轮询 host cancel；置位 → `ACS_ERR_CANCELLED`；
    host.cancel==NULL → detail 104；
  - `executor_lease`：acquire/release 归还；budget denied →
    `ACS_ERR_BUDGET`；host.executor==NULL → detail 103；workers 越界拒绝；
  - `config_query`：host config 查询（只读；FORBID-007）；host 无该服务 →
    `ACS_ERR_UNSUPPORTED`；
  - `request_error`：按 config `error_status`（1..70）触发稳定错误码/域映射；
- create/destroy 全程经 host allocator（12 §4 同一 allocator 分配/释放；
  artifact read_all 缓冲同样归还）；
- inspect 输出结构化诊断（metrics 形态：`exec_count`/`bytes_count`/
  `host_loaded`/`cancel`；ABI v1 host_api 无 metrics 回调字段 → inspect 上报）；
- 删除/替换本 DLL 后宿主无静态 fallback（ABI-005 验收失败路径）。

不负责（明确非本模块）：
- 任何科学/算法/数据处理（`SCI-NONE`/`ALG-NONE`/`DATA-NONE`）；
- loader（ABI-003）、registry（ABI-004）实现本身；本模块只作为它们的被加载
  对象与三方一致源之一；
- Windows 实机 DLL 加载验证（WIN-* 系列承担；本模块 Linux 同源 `.so` 语义
  等价验证）。

## 3. 输入与输出

无输入/输出 ports、无科学/算法/数据合同（conformance 模块；不接入 Phase
DAG）。input manifest / config 均为借入 JSON；输出为调用方提供的
`acs_strbuf_v1` 缓冲 JSON（截断语义：不足 → `ACS_ERR_PARAM` +
`BUFFER_TOO_SMALL`，`strbuf.size`=所需字节）。

## 4. 合同链接

- API：`API-ABI-001`（include/astrocs/abi/module_api_v1.h + host_api_v1.h +
  artifact_api_v1.h + lifecycle_v1.h，ABI-001/002 冻结）；
- 模块元数据合同：module.yaml 字段规范（11_MODULE_SOURCE_TEST_STANDARD.md
  §4）；三方（module.yaml / DLL descriptor / product manifest）一致校验由
  ABI-004 registry 机器执行；
- 安装树：packaging/install-tree.contract.json（BLD-003；本模块 add 后同步）。
- SCI/ALG/DATA：无（`SCI-NONE`/`ALG-NONE`；data_contracts 显式空）。

## 5. 实现事实

| 源 | 内容 |
|---|---|
| `src/echo_module.c` | 唯一导出 `astrocs_module_query_v1` + 静态 vtable + action 分发 |
| `include/astrocs/echo/types.h` | 静态标识常量 + config key/action + 自定义 detail 码（100..105） |
| `tests/unit/echo_host_callback_test.c` | 全部 host callback 正/负断言（独立 fake host；不调生产符号做 oracle） |

导出符号（`nm -D` 实测）：仅 `astrocs_module_query_v1`。vtable 全生命周期
回调齐全：describe/validate_config/plan/create/execute/inspect/
request_cancel/destroy。

config JSON action 白名单（validate/plan/execute 共享）：
`echo`（默认）、`artifact_read`、`write_blocked`、`cancel_poll`、
`executor_lease`、`config_query`、`request_error`。

## 6. 线程/并行/内存/I/O/取消/错误

- threading_model `host_executor_lease`：execute 内部不使用私有线程；如用
  host executor 只 acquire/release 归还（正测断言 acquired 回 0）；
- 同实例 execute 并发 → `ACS_ERR_STATE`（ABI-002 状态机；phase 字段保护）；
- 内存：跨边界全部经 host allocator（create 分配实例；destroy 同一
  allocator 释放；artifact read_all 缓冲同 allocator free；测试断言
  free==alloc 平衡）；
- I/O：全部经 host artifacts 服务；模块从不自行 fopen/打开路径（FORBID-002）；
- 取消：request_cancel 幂等置位 + host cancel 任一命中 → execute 安全点
  `ACS_ERR_CANCELLED`（domain=CANCELLED）；
- 错误：err.status/domain/detail_code 全部稳定；模块自定义 detail ≥100
  （lifecycle_v1.h §4 规则：通用 0..7，模块自定义 100 起）。

## 7. 验证

- 单元测试（共址 CTest）：`module:astrocs.conformance.echo` → 全部 host
  callback 正/负断言全 PASS；
- 动态加载（ABI-003 loader）：tests/abi/test_abi005_echo.py 经
  `abi003_loader_probe` 加载真实 `astrocs_echo.so`（绝对 canonical + sha256 +
  module_id + build_id 全校验一致）→ LOAD_OK/DESCRIBE_OK/RELEASE_OK；
  删除 DLL 后 registry 报缺（无静态 fallback）；
- exports：`nm -D --defined-only astrocs_echo.so` 仅
  `astrocs_module_query_v1`；
- 编译：`-Wall -Wextra -fno-exceptions` 零告警（Linux LIGHT；Windows 由
  WIN-* 验证）。

## 8. 已知限制 / 未实现项

- metrics 上报形态为 inspect JSON（host_api_v1 无 metrics 回调字段；ABI v1
  冻结面内模块无法主动上报计数）；
- Windows `astrocs_echo.dll` 构建/加载验证留 WIN-* 系列；
- product manifest 登记本模块（unit MOD-ECHO）与 install 树 add 属前台集成
  （BLD-003 install_layout 白名单同步），本任务不修改根 CMake/install 规则。
