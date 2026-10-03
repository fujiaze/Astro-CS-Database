# eng/packaging/ — 安装树与许可证布局

本目录是安装/打包布局的 owner 目录，存放安装树合同、产品 manifest、依赖锁、许可证收集与
Windows 工具链组件清单。

> 现态说明：本目录原先配套的打包面校验器（安装树验证器、打包一致性检查器、SBOM 输入生成器）
> 与安装/加载验证用例所在目录已退场，下文不再把它们列为现行可执行入口；现存件只有布局与合同数据。

| 路径 | 内容 |
|---|---|
| `install-tree.contract.json` | 安装树白名单合同（与 `eng/cmake/install_layout.cmake` install 规则一一对应） |
| `acsd.product.json` | 顶层产品 manifest 示例（安装在 `<prefix>/acsd.product.json`） |
| `dependency-lock.json` | 依赖锁 |
| `config/`、`launch/` | 随安装树分发的配置与启动脚本 |
| `schemas/install-tree-contract.schema.json` | 安装树合同 JSON Schema |
| `schemas/acsd-product.schema.json` | 产品 manifest JSON Schema |
| `schemas/dependency-lock.schema.json` | 依赖锁 JSON Schema |
| `schemas/preset-contract.json` | Windows preset 合同（工具链取值的机器单一事实源） |
| `licenses/` | 随安装树分发的许可证只读收集（第三方文本 + 索引） |
| `windows/` | Windows 工具链组件清单（另见其 README） |

## 安装（Linux）

```bash
cmake -S . -B build/linux-control -DCMAKE_BUILD_TYPE=Release   # 唯一根入口
cmake --build build/linux-control -j1
cmake --install build/linux-control --prefix <prefix>
```

安装一致性由 `install-tree.contract.json` 与 `eng/cmake/install_layout.cmake` 的 install 规则
一一对应保证；两者任一改动都必须同步另一方。
