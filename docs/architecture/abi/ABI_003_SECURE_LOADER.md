# Secure Module Loader (ABI-003)

> ID: DOC-ARCH-ABI-003 · owner: SA-ABI-03 · 状态: FROZEN (ABI-003, 2026-09-03)
> 上游: 12_DLL_ABI_AND_LOADER_STANDARD.md §6 / AstroCS_ENGINEERING_CONSTRAINTS.md §F3
>       / 03_TARGET_PRODUCT_AND_ARCHITECTURE.md（ARC-001 DLL 边界）
> 下游: ABI-004 动态 registry、ABI-005 conformance 探针、CLI-001 modules/doctor
> 实现: runtime/module_loader/（secure_loader.h + secure_loader.c）
> 测试: tests/abi/（abi003_loader_probe.c + test_secure_loader.py）
> 权威参考: include/astrocs/abi/module_api_v1.h / host_api_v1.h（ABI-001 冻结类型）

## 1. 目标与验收

实现受控动态加载: Windows 用受控绝对路径 +
`SetDefaultDllDirectories`/`AddDllDirectory`/`LoadLibraryExW` 安全 flags;
Linux 仅从 product manifest 的绝对 canonical path `dlopen`。加载前后校验
路径、hash、module ID、ABI/build ID。

验收（tasks/02_ABI_BUILD_CLI_TASKS.md ABI-003）:
1. 当前目录/PATH DLL 劫持拒绝 —— 相对路径入参即拒; 只认 manifest 绝对路径;
2. symlink escape 拒绝 —— canonical 不一致 + allowed_root 越界均拒;
3. hash mismatch 拒绝 —— manifest sha256 与实际文件不符;
4. 缺 symbol 拒绝 —— 无 `astrocs_module_query_v1` 导出;
5. wrong arch/ABI 拒绝 —— 非 ELF / ELF32 / 非 x86-64 / host_abi 失配 / ABI 版本不符;
6. 日志不泄凭据 —— 错误消息为静态字面量, 不含路径/sha/内容; loader 不写日志文件;
7. 正测 —— 加载 BLD-003 真实 noop 模块, module_id/version/build/hash 三方一致。

## 2. 信任边界与加载序

```text
product manifest(host 解析) → unit 记录(绝对路径/sha/module_id/abi/build_id)
  → [1] 路径: 绝对 + realpath canonical + allowed_root 前缀
  → [2] ELF64-x86-64 头校验 + sha256 比对(manifest 期望)
  → [3] dlopen(canon, RTLD_NOW|RTLD_LOCAL)   [Windows: LoadLibraryExW 安全 flags]
  → [4] 必需入口符号存在 + host_abi 握手(ACS_ABI_VERSION_V1)
  → [5] describe → head/abi_version/module_id/build_id/version 校验
  → [6] 句柄返回; 失败一律 *out=NULL + 非 0 + err(detail_code), 绝不 fallback
```

关键点:
- loader **不做** manifest JSON 解析(12 §3: module 不自行开任意路径); host 解析
  product manifest 后填 `acs_load_manifest_unit_v1`(ABI-004 接线);
- 拒绝相对路径 → 当前目录/PATH/`LD_LIBRARY_PATH` 发现语义不存在;
- `realpath` 后再比对入参文本: 入参含 symlink/`..` 分量即拒 → 目录内链接伪装
  无法把 loader 导向白名单外文件;
- `allowed_root` 自身 canonical 后做目录前缀比对 → symlink escape 拒;
- sha256 为 loader 内部 FIPS 180-4 实现(纯 C 自包含, 无第三方依赖; 二进制
  身份比对用途), 已用 FIPS 附录 B 向量自检;
- dlopen 前已整文件读入内存校验 hash; dlopen 只接受 canonical 绝对路径。

## 3. Windows 契约(实现由 WIN-* 落地)

本头冻结两平台一致的语义与错误码; Windows 落地实现应:
1. UTF-8 路径 → UTF-16(canonical 化: GetFinalPathNameByHandleW 后比对);
2. `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
   LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32)`;
3. manifest 授权目录经 `AddDllDirectory` 后
   `LoadLibraryExW(utf16_abs, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
   LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32)`;
4. PE 头校验(PE32+/AMD64/子系统) + Authenticode 或 sha256 登记比对;
5. 加载后 GetProcAddress 入口符号 + 握手 + describe 校验(与本文件同序);
6. 禁止: 当前目录/PATH/SearchPath/用户数据目录发现, 无 fallback 静态算法。

`secure_loader.c` 在 `_WIN32` 下返回 ACS_ERR_UNSUPPORTED, 不伪装实现完成。

## 4. 错误模型

状态码映射(数值=status_codes.h 冻结; detail_code 为权威细分, 见
`secure_loader.h` 枚举):
- `ACS_ERR_PARAM`: 非绝对/非 canonical/escape/kind 不支持/hash 失配(文件本身
  合法但登记不符) —— detail 2/3/4/5/17;
- `ACS_ERR_IO`: 文件缺失/不可读/dlopen 失败 —— detail 6/7/18;
- `ACS_ERR_ABI_MISMATCH`: ELF 格式/架构、缺 symbol、握手、descriptor/module_id/
  build_id 失配 —— detail 8-16;
- `ACS_ERR_NOMEM` / `ACS_ERR_UNSUPPORTED`(_WIN32 占位) / `ACS_ERR_INTERNAL`。

## 5. 日志纪律

- 错误消息为编译期静态字面量(`detail_message()`), 无格式化参数 → 无注入面;
- loader 不写任何日志文件; 详细诊断(路径等)经 `acs_error_info_v1` 返回调用方;
- 测试 L1/L2/L3 静态断言: 消息无 `%` 插值、loader 无 fopen 写模式/独立 open、
  拒绝输出不含路径与 sha 前缀。

## 6. 测试清单(36 checks)

| 组 | 场景 | 断言 |
|---|---|---|
| P1 | 绝对 canonical + 正确 sha/mid/build | LOAD_OK + describe + release |
| P2 | manifest 未登记 sha(SKELETON 兼容) | LOAD_OK |
| P3 | 真实 noop 模块(module.yaml 三方一致) | LOAD_OK, mid/version/build/sha 全符 |
| P3b | noop + manifest 错误 sha | detail=5 |
| N1 | 相对路径(当前目录劫持) | detail=2 PATH_NOT_ABS |
| N2 | 篡改文件 hash 失配 | detail=5 |
| N3 | 缺入口 symbol | detail=11 |
| N4 | host_abi 握手拒 | detail=12 |
| N5 | manifest module_id 不符 | detail=15 |
| N6 | manifest build_id 不符 | detail=16 |
| N7a | symlink 路径 | detail=3 |
| N7b | allowed_root 外 | detail=4 |
| N8a/b/c | 非 ELF / ELF32 / 非 x86-64 | detail=8/9/10 |
| N9 | manifest abi_version=99 | detail=14 |
| N10 | 文件缺失 | detail=6 |
| N11 | kind 不支持 | detail=17 |
| L1-3 | 消息字面量/无文件写/拒绝输出无泄露 | 静态断言 |

运行: `python3 tests/abi/test_secure_loader.py`(gcc + -ldl; 内部编译 fixture
与 noop 模块, 不依赖仓库先 build)。

## 7. 非目标(不伪装完成)

- Windows LoadLibraryExW 实机验证 → WIN-*;
- product manifest 解析/registry 接入/三方校验 → ABI-004;
- conformance 模块语义(validate/plan/create/execute…) → ABI-005;
- provider 加载后 CPUID/self_test 路由 → CPU-002/ABI-004。
