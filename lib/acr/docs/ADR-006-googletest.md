# ADR-006: GoogleTest v1.15.2 测试框架

| 项目 | 值 |
| --- | --- |
| 状态 | Accepted |
| 日期 | 2026-08-02 |
| 依赖锁 | `lib/acr/docs/dependency-lock.json` |
| 版本 | GoogleTest v1.15.2（commit `v1.15.2`） |
| 许可证 | BSD-3-Clause |
| 平台 | Windows / Linux |
| 形态 | 编译型库 |

## 状态

Accepted。GoogleTest 作为 ACR 唯一的单元测试框架。仓库现有测试未使用任何框架，控制包要求 ACR 必须选定一个测试框架，并明确禁止 Catch2 与 GoogleTest 双引入。

## 背景

ACR 模块需要单元测试覆盖：API 契约、容差比较（浮点）、故障注入（错误路径）、schema 校验（JSON 路由表/指纹）。仓库历史代码无测试框架，新模块必须从零引入。

候选框架：

- **GoogleTest**：与 ADR-005 Google Benchmark 同生态（构建链、CI、MSYS2 包一致），表达式断言宏（`EXPECT_NEAR` / `EXPECT_THAT` + matcher）成熟，文档与社区丰富。
- **Catch2 v3**：header-only 友好，但与 Google Benchmark 生态分离，需维护两套构建配置；表达式宏虽好但与 ACR 选定的 Benchmark 不一致。

控制包明确禁止 Catch2 + GoogleTest 双引入（避免测试框架分裂），故必须二选一。

## 决策

1. 引入 GoogleTest v1.15.2 作为 ACR 唯一单元测试框架。
2. 不引入 Catch2，禁止任何模块以 Catch2 编写测试。
3. 测试覆盖维度：API 单测、容差比较、故障注入、schema 校验。
4. 与 Google Benchmark 共享构建链（同一 FetchContent 缓存区，ADR-008）。
5. 测试可执行文件命名：`acr_<module>_test`，输出到 `build/bin/`。

## 理由

- 与 Google Benchmark 同生态，构建与 CI 集成零额外成本。
- MSYS2 有预编译包，Windows 构建无障碍。
- BSD-3 许可证宽松。
- `EXPECT_NEAR` / `ASSERT_NEAR` 原生支持浮点容差，契合 ACR 数值测试需求。
- `EXPECT_THROW` 原生支持故障注入测试。
- 表达式宏与 matcher 成熟，可读性高。
- 文档与社区规模远超 Catch2，问题排查成本低。

## 集成边界

- **职责内**：单元测试驱动、断言宏、 fixture、参数化测试、死亡测试、schema 校验辅助。
- **职责外**：性能测量（由 ADR-005 Google Benchmark 承担）、集成测试（由 ACR 顶层 orchestrator 驱动）。
- **生态边界**：与 Google Benchmark 共享 CMake 构建链，但不共享可执行文件；测试与 benchmark 分属不同 target。
- **API 边界**：`testing::Test` 等 GoogleTest 类型不得出现在 ACR 公共 API 签名中（测试代码不属公共 API）。
- **构建边界**：仅在 `ACR_BUILD_TESTS=ON` 时编译，默认 ON；CPU-only 构建强制可用。

## 替代方案

1. **Catch2 v3**：
   - 未采用：与 Google Benchmark 生态分离，维护两套构建配置成本高；控制包禁止双框架引入。
2. **自写 `assert` + 手工 main**：
   - 未采用：无 fixture、无参数化、无容差宏、无死亡测试；长期不可维护。
3. **doctest**：
   - 未采用：header-only 友好但生态规模小于 GoogleTest；与 Google Benchmark 不属同生态。

## 未采用原因

Catch2 与 Google Benchmark 生态分离且被控制包禁止双引入；自写 assert 在 fixture/参数化/容差上不达标；doctest 生态规模不足以支撑长期维护。

## 验收实验

| 实验 | 目标 | 通过条件 |
| --- | --- | --- |
| API 单测 | 每个 ACR 公共 API 至少一条测试 | 覆盖率报告 ≥ 80% 行覆盖 |
| 容差比较 | 浮点结果比较 | `EXPECT_NEAR` 容差 1e-6 通过 |
| 故障注入 | 错误路径覆盖 | `EXPECT_THROW` 验证异常类型与消息 |
| schema 测试 | 路由表/指纹 JSON 校验 | 非法 schema 测试用例失败，合法用例通过 |
| 跨平台 | Windows + Linux 均运行测试 | MSVC + GCC/Clang 全部用例通过 |

测试日志写入 `run/logs/acr/gtest/<YYYYMMDD>/`。
