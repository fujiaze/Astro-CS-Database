# 新树清理与追溯

基线c5fee09fedb97ccbe39cb47ea0985e0d8a339db2。main及Git历史不变。重构分支移除4295个旧跟踪文件，全部路径、基线Git blob和理由见[removed-files.csv](removed-files.csv)。可用git show <baseline>:<path>追溯，清单不是运行依赖。

| 理由 | 处理 |
|---|---|
| old-production-source | lib/全部旧算法/Runtime/ARS/ABI/内部对象/vendor源；新实现不复制 |
| old-build-test-config-script | eng/、.github/、config/、run/、testdata/与旧调试脚本；新验证不依赖旧测试 |
| superseded-documentation | 旧docs/detail/architecture/重复分册；保留五份foundation与六份rebuild并补齐 |
| historical-research-not-new-validation | 实验/、artifacts/移出新树；固定提交外部链接保留独立证据，不作新测试 |

旧许可证保留third_party/historical-notices，根README/AGENTS/CMake/Presets/VERSION重写。清理只枚举git跟踪文件、验证resolved路径位于仓库，不递归删除其他工作区、sources或未跟踪文件。空旧目录不入Git。

科学定义与适用域保持；历史复算命令只在固定历史提交checkout有效。旧源静态反例不要求新适配器。所有新本地引用用check_design核查，历史引用固定提交或明确一手文献。删除、设计、审核修订分别可追溯提交，不改写历史。
