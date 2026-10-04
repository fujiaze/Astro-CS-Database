# 安全装载器合同

上游：最高设计的 CPU 后端与资源、模块与 ABI 两章；C ABI 基础层见 `ABI.md`。

动态库加载前的六查、信任边界、装载失败语义与清单校验的正本。

## 目标与验收

实现面 = `lib/infrastructure/pipeline/module_loader/`（`secure_loader.h` + `secure_loader.c`）；
类型正本 = `lib/include/acsd/abi/module_api_v1.h` / `host_api_v1.h`。本层不随仓内留测试脚本载体，
装配 fixture 由该验收面自行构建。
本层提供受控动态加载： Windows 用受控绝对路径 +
`SetDefaultDllDirectories`/`AddDllDirectory`/`LoadLibraryExW` 安全 flags;
Linux 仅从 product manifest 的绝对 canonical path `dlopen`。加载前后校验
路径、hash、module ID、ABI/build ID。

验收（依据=`../../../ACSD_DESIGN.md` 「模块与 ABI」一节/+ 本文件）:
1. 当前目录/PATH DLL 劫持拒绝 —— 相对路径入参即拒; 只认 manifest 绝对路径;
2. symlink escape 拒绝 —— canonical 不一致 + allowed_root 越界均拒;
3. hash mismatch 拒绝 —— manifest sha256 与实际文件不符;
4. 缺 symbol 拒绝 —— 无 `acsd_module_query_v1` 导出;
5. wrong arch/ABI 拒绝 —— 非 ELF / ELF32 / 非 x86-64 / host_abi 失配 / ABI 版本不符;
6. 日志不泄凭据 —— 错误消息为静态字面量, 不含路径/sha/内容; loader 不写日志文件;
7. 正测 —— 加载 `eng/tests/conformance/noop/` 真实一致性模块, module_id/version/build/hash 三方一致。

## 信任边界与加载序

```text
product manifest(host 解析) → unit 记录(绝对路径/sha/module_id/abi/build_id)
 → [1] 路径: 绝对 + realpath canonical + allowed_root 前缀
 → [2] ELF64-x86-64 头校验 + sha256 比对(manifest 期望)
 → [3] dlopen(canon, RTLD_NOW|RTLD_LOCAL) [Windows: LoadLibraryExW 安全 flags]
 → [4] 必需入口符号存在 + host_abi 握手(ACS_ABI_VERSION_V1)
 → [5] describe → head/abi_version/module_id/build_id/version 校验
 → [6] 句柄返回; 失败一律 *out=NULL + 非 0 + err(detail_code), 绝不 fallback
```

关键点:
- 职责划分：host 解析 product manifest（模块不自行开任意路径）后填 `acsd_load_manifest_unit_v1`，
 loader 消费该已解析单元记录；动态 registry 接线见 `module_loader/module_registry.h`、`module_registry.c`。
- 拒绝相对路径 → 当前目录/PATH/`LD_LIBRARY_PATH` 发现语义不存在;
- `realpath` 后再比对入参文本: 入参含 symlink/`..` 分量即拒 → 目录内链接伪装
 无法把 loader 导向白名单外文件;
- `allowed_root` 自身 canonical 后做目录前缀比对 → symlink escape 拒;
- sha256 为 loader 内部 FIPS 180-4 实现(纯 C 自包含, 无第三方依赖; 二进制
 身份比对用途), 判定向量取自 FIPS 180-4 附录 B;
- hash 校验在 dlopen 前完成(整文件读入内存校验); dlopen 只接受 canonical 绝对路径。

## Windows 契约

本头规定两平台一致的语义与错误码，Windows 落地实现按下列步骤:
1. UTF-8 路径 → UTF-16(canonical 化: GetFinalPathNameByHandleW 后比对);
2. `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
 LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32)`;
3. manifest 授权目录经 `AddDllDirectory` 后
 `LoadLibraryExW(utf16_abs, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
 LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32)`;
4. PE 头校验(PE32+/AMD64/子系统) + Authenticode 或 sha256 登记比对;
5. 加载后 GetProcAddress 入口符号 + 握手 + describe 校验(与本文件同序);
6. 发现范围: 仅限白名单目录(应用目录/System32), 解析算法 = 静态白名单。

`secure_loader.c` 在 `_WIN32` 下返回 `ACS_ERR_UNSUPPORTED`，该分支属发行验证面。

## 错误模型

状态码映射(数值=status_codes.h 冻结; detail_code 为权威细分, 见
`secure_loader.h` 枚举):
- `ACS_ERR_PARAM`: 非绝对/非 canonical/escape/kind 不支持/hash 失配(文件本身
 合法但登记不符) —— detail 2/3/4/5/17;
- `ACS_ERR_IO`: 文件缺失/不可读/dlopen 失败 —— detail 6/7/18;
- `ACS_ERR_ABI_MISMATCH`: ELF 格式/架构、缺 symbol、握手、descriptor/module_id/
 build_id 失配 —— detail 8-16;
- `ACS_ERR_NOMEM` / `ACS_ERR_UNSUPPORTED`(_WIN32 占位) / `ACS_ERR_INTERNAL`。

## 日志纪律

- 错误消息为编译期静态字面量(`detail_message`), 无格式化参数 → 无注入面;
- loader 不写任何日志文件; 详细诊断(路径等)经 `acsd_error_info_v1` 返回调用方;
- 测试静态断言（L1/L2/L3）: 消息无 `%` 插值、loader 无 fopen 写模式/独立 open、
 拒绝输出不含路径与 sha 前缀。

## 测试清单

| 组 | 场景 | 断言 |
|---|---|---|
| P1 | 绝对 canonical + 正确 sha/mid/build | LOAD_OK + describe + release |
| P2 | manifest 未登记 sha(骨架模块) | LOAD_OK |
| P3 | 一致性模块(`eng/tests/conformance/noop/module.yaml` 三方一致) | LOAD_OK, mid/version/build/sha 全符 |
| P3b | 一致性模块 + manifest 错误 sha | detail=5 |
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

上表逐条对应本篇拒绝模型的 detail 码，是安全装载器在 Linux 技术预览面上的验收矩阵；
负路径与正路径的装配 fixture 由该面自行构建，不在仓内留脚本载体。

## 非目标

- Windows `LoadLibraryExW` 实机验证（发行验证面）;
- product manifest 解析与动态 registry 接线（`module_loader/module_registry.h`）;
- conformance 模块语义(validate/plan/create/execute…) 探针（`eng/tests/conformance/noop/` 只装配骨架，不含语义探针）;
- provider 加载后的 CPUID/self_test 路由（`lib/infrastructure/benchmark/cpu/` 与安全 loader 的接线）。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/api/abi/ABI.md`，同层相关正本。
[3] 内部文档 `docs/engineering/resources/cpu/BACKEND.md`，同层相关正本。
