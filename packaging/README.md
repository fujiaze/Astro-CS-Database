# packaging/ — 安装树与许可证布局 (BLD-003)

本目录是 SA-BLD-02 的安装/打包布局 owner 目录。职责：

| 文件 | 内容 |
|---|---|
| `install-tree.contract.json` | 安装树白名单合同（与 `cmake/install_layout.cmake` install 规则一一对应；机器校验入口 `verify_install_tree.py`） |
| `astrocs.product.json` | 顶层产品 manifest 示例（安装在 `<prefix>/astrocs.product.json`；ABI-004 完善 hash/entry 三方校验） |
| `schemas/install-tree-contract.schema.json` | 安装树合同 JSON Schema |
| `schemas/astrocs-product.schema.json` | 产品 manifest JSON Schema |
| `verify_install_tree.py` | 安装树/module verify 验证器（BLD-003 验收：缺 noop → 明确失败；无静态 fallback；W5-PKG-001 增补：白名单外多列文件、合同↔manifest 单元集、manifest sha256 三项判据 + `--self-test` 负例面） |
| `check_packaging_consistency.py` | 打包面一致性检查器（W5-PKG-001：版本单源 C1 / 安装树闭包 C2 / 双平台清单同构 C3 / 依赖锁↔实树 C4 / 依赖锁↔CMake C5 / 安装规则↔合同 C6 / 许可登记 C7；`--self-test` 为可执行负例面） |
| `gen_sbom_input.py` | SBOM 输入生成/校验器（BLD-004：DEPENDENCIES.md 交叉 + 机器路径扫描 + vendored 实树 hash 复核；`--self-test` 为可执行负例面） |
| `licenses/` | 随安装树分发的许可证只读收集（第三方文本 + 索引；自身许可证声明缺历史根 LICENSE，由 BLD-004/GOV 裁定） |
| `windows/` | Windows 工具链组件清单（BLD-001，另见其 README） |

## 安装/验证命令（Linux 技术预览）

```bash
cmake -S . -B build/linux-control -DCMAKE_BUILD_TYPE=Release   # 唯一根入口
cmake --build build/linux-control --target astrocs_runtime astrocs_io \
      astrocs_noop astrocs_cpu_baseline astrocs_catalog_gaia \
      astrocs_p1_drizzle astrocs_p1_calibration astrocs_p1_cosmetic \
      astrocs_p1_hips_writer -j1
cmake --install build/linux-control --prefix <prefix>
python3 packaging/verify_install_tree.py --prefix <prefix>       # 全 required 在 + 无白名单外条目 → PASS
rm <prefix>/modules/astrocs_noop.so                              # 删除模块 DLL
python3 packaging/verify_install_tree.py --prefix <prefix>       # → 非零 + MODULE VERIFY FAIL
python3 packaging/verify_install_tree.py --self-test             # 负例注入必须判红（§8 可执行负例面）
python3 packaging/check_packaging_consistency.py --root .        # 登记面一致性（C1-C7）
python3 packaging/check_packaging_consistency.py --root . --self-test
python3 tests/abi/mod001_install_load_check.py --build-dir build/linux-control \
      --keep                                                     # MOD-001 安装+安全 loader 逐 unit 加载验证
```

MOD-001（科学DLL安装加载验证与产品清单）：required 集自 BLD-003 的 6 项扩至
11 项（+5 科学模块 DLL，宪章 §8.4）；安全 loader（§18.4 签名清单官方模块）
逐 unit 加载验证与负向注入见 `tests/abi/mod001_install_load_check.py`。
F-CI-002-01（owner 裁决 2026-09-11）：astrocs_p1_noise 随 lib/snr_estimator
V7 残留断链解除摘出生产图与本清单（未入库不登记），V7 残留收编后恢复。

Windows 正式安装树（`AstroCS-<根 VERSION>-win-x64/`，03_TARGET §4）由
WIN-* 系列在 Fatduck 验证；本目录同步声明布局，不在 Linux 伪装 Windows 结论
（10_LINUX_CONTROL_NODE.md §5 PLATFORM_SCOPE）。
