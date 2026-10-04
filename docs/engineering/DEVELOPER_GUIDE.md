# Astro Celestial Sphere Database（ACSD） 开发者指南

> 上游：docs/ACSD_DESIGN.md §8（软件架构）、§10（I/O 与原子产品）、§11（双平台发行）、§12（验证体系与四层验收）；AGENTS.md §3（环境与构建）、§8（提交纪律）；docs/engineering/CODE_STANDARD.md（代码风格与实现处置）、docs/engineering/TEST_STANDARD.md（测试规范）。

## 环境与构建

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

模块地图见 `docs/engineering/MODULE_MAP.md`；验证的执行范围、改动集与分档口径见 `docs/engineering/TEST_STANDARD.md` §8。

## 测试

测试面随模块在位：共址测试放在该模块自己的测试面里，与实现同处一个模块目录，
改动与验证同批完成。构建按上文两条命令走完即可，测试用例由验证者按改动集点名执行。

验证的判据来源是科学正本（`docs/science/`）与本目录各合同 —— 科学契约、确定性要求、
覆盖率分母与回归集合见 `docs/engineering/TEST_STANDARD.md` §1–§5；
执行范围、改动集与分档见同文件 §8。科学 Oracle 与科学不变量两类判据必须有
可复算的实验证据（复现命令 + 实测读数 + 产物路径），证据齐备才认作通过；
判据必须能红 —— 注入缺陷时读数不随之变红即判红。

## 编码与提交

- 编码规范、测试规范与提交纪律的唯一正本 = `docs/engineering/CODE_STANDARD.md`、`docs/engineering/TEST_STANDARD.md`、`AGENTS.md` §8（本指南不复述）；
- 运行产物只写 `output_dir`；过程产物与日志落点按 `docs/ACSD_DESIGN.md` §10（I/O 与原子产品）的运行产物规则；
- 修改后必须自测；「完成」以验证通过为前提。

## 参考面

- 标准与锚合同：`docs/detail/anchors/ANCHOR_CONTRACT.md`；
- 追溯：`docs/engineering/TRACEABILITY_SPEC.md §9`（机器真相）、`docs/engineering/TRACEABILITY_SPEC.md`（合同）。
