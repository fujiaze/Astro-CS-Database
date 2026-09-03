# runtime/module_loader — Secure Loader (ABI-003)

Linux 真实实现的安全动态加载器; Windows 契约同源(实现在 WIN-* 落地)。

## 文件

| 文件 | 角色 |
|---|---|
| `secure_loader.h` | 合同头(固定宽度 POD、head、错误码枚举、UTF-8 span、opaque handle; 与 `include/astrocs/abi/` 冻结风格一致) |
| `secure_loader.c` | Linux 实现: canonical 路径 + ELF64 校验 + FIPS 180-4 sha256 + dlopen + 加载后符号/握手/describe 校验 |
| `tests/abi/abi003_loader_probe.c` | 验收探针(包装 load/describe/release, 输出机器可读结果) |
| `tests/abi/test_secure_loader.py` | 全部正/负场景编排(36 checks) |

## 语义(12_DLL_ABI_AND_LOADER_STANDARD.md §6)

- 路径必须**绝对**; `realpath` canonical; canonical 与入参不一致 → 拒(防目录内链接);
- 可选 `allowed_root` 前缀校验 → symlink escape 拒;
- 读文件: ELF64-x86-64 头校验 + sha256(FIPS 180-4 自实现)与 manifest 比对;
- `dlopen(canon, RTLD_NOW|RTLD_LOCAL)`; 不读 LD_LIBRARY_PATH、不 fallback;
- 加载后: 必需入口符号 → host_abi 握手 → describe → module_id/build_id/version 三方一致;
- 日志纪律: 错误消息为静态字面量, 不含路径/sha/内容; loader 不写任何日志文件。

## 使用

```c
acs_load_manifest_unit_v1 unit = { .head = {sizeof(unit), 1}, .kind = sv("module"),
  .abs_path_utf8 = sv(manifest_path), .module_id = sv("astrocs.conformance.noop"),
  .expected_sha256 = sv(hex64), .expected_build_id = sv(build_id), .abi_version = 1 };
acs_loader_options_v1 opt = { .head = {sizeof(opt), 1}, .unit = unit,
  .allowed_root_utf8 = sv(install_root), .allocator = &alloc };
acs_error_info_v1 err; acs_loader_handle* h = NULL;
acs_status st = acs_secure_loader_load_v1(&opt, &err, &h);
/* 成功 → describe_v1 读取 module_api; 用完 release_v1 */
```

## 运行测试

```bash
python3 tests/abi/test_secure_loader.py   # 退出码 0 = 36/36 PASS
```

## 状态

- 本文件族由 ABI-003 交付: 加载器安全语义冻结; host(registry) 接线属 ABI-004
  (product manifest → unit 记录 → load)。
- Windows `LoadLibraryExW` 实现由 WIN-* 按本头契约落地(SetDefaultDllDirectories/
  AddDllDirectory/LOAD_LIBRARY_SEARCH_*); `_WIN32` 分支当前返回
  ACS_ERR_UNSUPPORTED, 不伪装完成。
