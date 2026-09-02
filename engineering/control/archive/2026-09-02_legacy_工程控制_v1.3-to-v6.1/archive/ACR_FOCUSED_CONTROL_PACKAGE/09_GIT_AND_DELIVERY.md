# Git与交付规则

## 1. 分支

- 唯一分支：`feature/astrocompute-runtime`；
- 不新建日期、V2、rewrite或临时交付分支；
- 不新建仓库；
- 提交保持小而可审计。

建议提交顺序：

1. `refactor(acr): focus runtime on target pixel operations`
2. `bench(acr): add focused operation profiles`
3. `feat(acr): tune mixed chunk routing and tail gating`
4. `feat(acr): reuse residency and finish memory budget`
5. `test(acr): add focused qualification evidence`

## 2. Path guard

任何提交前检查：

- 未修改真实算法目录；
- 未修改现有OpenMP路径；
- 未修改Pipeline、Stage1/2和正常CLI；
- 未引入新仓库/分支/大体积构建产物。

失败必须终止，不能在报告中把ABORT写成PASS。

## 3. Evidence

- 在仓库外生成，不创建Evidence commit；
- `git rev-parse HEAD`、源码快照和日志tip一致；
- `git status --porcelain`为空；
- 保存完整命令、cwd、环境、timeout、exit code和原始日志；
- 不混入旧HEAD证据；
- UTF-8路径SHA-256可复核。

## 4. 交付内容

只包含必要内容：

- 控制包同步证明；
- 完整源码快照；
- 最小diff/patch；
- OperationProfile与原始Benchmark；
- 测试和Sanitizer日志；
- Mixed/residency/memory报告；
- manifest与SHA256。

禁止携带build目录、依赖缓存、重复源码树和无关大文件。

## 5. 合并

全部底层门禁通过后才允许`--no-ff`合并到`main`。合并后ACR保持dormant，普通AstroCS启动零副作用。
