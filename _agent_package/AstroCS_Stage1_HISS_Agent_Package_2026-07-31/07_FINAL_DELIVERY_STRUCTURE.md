# Agent 最终交付 ZIP 结构

## 总原则

交付必须小，只包含应用到旧仓库所需的新增/修改/删除信息和报告。不要压缩整个仓库。

推荐结构：

```text
AstroCS_Stage1_HISS_Delivery/
├─ 00_README.md
├─ APPLY_IN_PLACE.md
├─ MANIFEST.json
├─ MANIFEST.sha256
├─ DELETE_LIST.txt
├─ git_diff.patch
├─ changed_files/
│  └─ <保持仓库相对路径，仅含新增或修改文件>
├─ wiki/
│  └─ <仅含新增或修改的Wiki页面>
├─ tests/
│  └─ <仅含新增或修改测试；若已在changed_files中则不重复>
└─ reports/
   ├─ repository_audit.md
   ├─ implementation_summary.md
   ├─ correctness_report.md
   ├─ performance_profile.md
   ├─ decision_queue.md
   └─ experiments/
      ├─ summary.md
      ├─ environment.md
      ├─ raw_results.csv
      └─ raw_results.json
```

## 必须内容

### `00_README.md`

- 本次实现了什么；
- 哪些仍未冻结；
- 构建和测试入口；
- 明确未运行Stage2/710。

### `APPLY_IN_PLACE.md`

- 目标仓库基准commit；
- 如何应用patch或复制changed_files；
- 删除哪些旧文件；
- 如何构建；
- 如何运行代表性测试；
- 所有命令的超时/终止条件。

### `DELETE_LIST.txt`

列出应从旧仓库删除的无用原型、冲突文档和重复实现。不得仅在新包中“忽略”旧文件。

### `git_diff.patch`

基于审计时的旧仓库提交生成，方便快速检查和应用。

### `changed_files/`

保留仓库相对路径，只包含必要源码、构建配置和文档。若patch足够，可不重复大型文件，但新增二进制资源原则上不应存在。

### `reports/decision_queue.md`

至少列出：

- 各子块最终默认codec/transform；
- FULL/BITMAP/SPARSE最终切换阈值；
- checksum算法；
- 子块对齐；
- 其他尚未由用户冻结的工程选项。

每项必须附实验报告位置，不能自行标记“已决定”。

## 禁止打包

- 完整旧仓库；
- `.git/`；
- build、dist、bin、obj；
- 编译器缓存；
- 原始天文测试数据；
- HISS大样本；
- Python环境；
- IDE配置缓存；
- 数百MB日志；
- 重复副本；
- Stage2输出；
- 710回归结果。

## ZIP体积控制

正常交付应以源码、Wiki、patch和文本/CSV/JSON报告为主。若结果异常大，先检查是否误打包仓库、测试数据或构建产物。
