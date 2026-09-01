# DOC-004: 补齐所有生产模块 README

任务 ID: DOC-004
Gate: G7
依赖: DOC-001
平台: Linux
变更类别: documentation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` DOC-004：

> 每个 Registry production module 一份 README：做/不做、输入输出 DATA 和单位、
> SCI/ALG/API/MOD/TEST 链接、public header/核心 symbol、execution class/parallel
> axis/lease/determinism、memory/cache/ownership、error/cancel/checkpoint、
> synthetic command、限制。checker 以 registry 为源，不能手工写"只需 5 个"。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 每个 Registry production module 一份 README | 22 个 registry 模块(module_adapters.cpp 唯一源) → docs/modules/registry/<id>.md 22 份 | c01 |
| 以 registry 为源 | 生成器 gen_module_readmes.py 从 descriptor 提取 module_id/execution_class/parallel_ok/ports/SCI/ALG/API/TEST | c01 |
| front matter(id/version/status/owner/source_commit/upstream/downstream) | 22/22 README 含完整 front matter | c01 |
| 职责/端口/DATA/单位/链接/execution class/内存/错误/synthetic/限制 | 每份 README 按模板章节完整(端口表含 DATA/必可/单位/坐标) | c01 |

## 实现文件

- `tools/quality/gen_module_readmes.py`（新）：从 module_adapters.cpp 提取 descriptor → README 生成器
- `docs/modules/registry/*.md`（新，22 份）：astrocs.phase{1,2,3}.* 模块 README

## 测试结果

- c01: MODULE_README_GEN_PASS modules=22, front matter 22/22 完整

## 说明

- 生成器为 checker 源(非手工清单); 改动 registry descriptor 后重跑生成器即同步。
- 旧 docs/modules/*.md(库级 V5)保留不动; 新 registry 级 README 独立目录。
