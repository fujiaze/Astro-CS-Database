# Astro Celestial Sphere Database（ACSD） 开发者指南

> 上游：docs/ASTROCS_DESIGN.md §8（软件架构）、§10（I/O 与原子产品）、§12（验证体系与四层验收）；AGENTS.md §3（环境与构建）；ENGINEERING_SPEC.md §1（语言/编译器/平台）、§2（代码风格与实现处置）、§5（测试规范）、§6（Git 与提交）。

## 环境与构建

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

模块地图见 `docs/engineering/MODULE_MAP.md`；机器一致性检查见 `docs/engineering/CI_SPEC.md`（范围与门禁口径 = 该文件 §2）。

## 测试

```bash
ctest --test-dir build --output-on-failure
```

模块级科学矩阵由 `eng/tests/**` 的 CTEST 目标现场枚举（唯一源 = 构建面与 `eng/ci/checks.json` 的 `ctest_targets` 字段），本指南只给入口：

```bash
ctest --test-dir build -R <target> --output-on-failure      # 单个矩阵
python3 eng/ci/run_checks.py --check CHK-ORACLE --quiet     # 科学 Oracle 面
python3 eng/ci/run_checks.py --check CHK-INVARIANT --quiet  # 科学不变量面
```

## 编码与提交

- 编码规范、测试规范与提交纪律的唯一正本 = `ENGINEERING_SPEC.md` §2 / §5 / §6（本指南不复述）；
- 运行产物只写 `output_dir`；过程产物与日志落点按 `ENGINEERING_SPEC.md` §7 的运行产物规则；
- 修改后必须自测；「完成」以验证通过为前提。

## 参考面

- 标准与锚合同：`docs/standards/`、`docs/detail/anchors/ANCHOR_CONTRACT.md`；
- 追溯：`docs/traceability/TRACEABILITY_MATRIX.json`（机器真相）、`docs/engineering/TRACEABILITY_SPEC.md`（合同）。
