# packaging/ — 安装树与许可证布局 (BLD-003)

本目录是 SA-BLD-02 的安装/打包布局 owner 目录。职责：

| 文件 | 内容 |
|---|---|
| `install-tree.contract.json` | 安装树白名单合同（与 `cmake/install_layout.cmake` install 规则一一对应；机器校验入口 `verify_install_tree.py`） |
| `astrocs.product.json` | 顶层产品 manifest 示例（安装在 `<prefix>/astrocs.product.json`；ABI-004 完善 hash/entry 三方校验） |
| `schemas/install-tree-contract.schema.json` | 安装树合同 JSON Schema |
| `schemas/astrocs-product.schema.json` | 产品 manifest JSON Schema |
| `verify_install_tree.py` | 安装树/module verify 验证器（BLD-003 验收：缺 noop → 明确失败；无静态 fallback） |
| `licenses/` | 随安装树分发的许可证只读收集（第三方文本 + 索引；自身许可证声明缺历史根 LICENSE，由 BLD-004/GOV 裁定） |
| `windows/` | Windows 工具链组件清单（BLD-001，另见其 README） |

## 安装/验证命令（Linux 技术预览）

```bash
cmake -S . -B build/linux-control -DCMAKE_BUILD_TYPE=Release   # 唯一根入口
cmake --build build/linux-control --target astrocs_runtime astrocs_io \
      astrocs_noop astrocs_cpu_baseline -j1
cmake --install build/linux-control --prefix <prefix>
python3 packaging/verify_install_tree.py --prefix <prefix>       # 全 required 在 → PASS
rm <prefix>/modules/astrocs_noop.so                              # 删除 noop
python3 packaging/verify_install_tree.py --prefix <prefix>       # → 非零 + MODULE VERIFY FAIL
```

Windows 正式安装树（`AstroCS-0.11.0-alpha.1-win-x64/`，03_TARGET §4）由
WIN-* 系列在 Fatduck 验证；本目录同步声明布局，不在 Linux 伪装 Windows 结论
（10_LINUX_CONTROL_NODE.md §5 PLATFORM_SCOPE）。
