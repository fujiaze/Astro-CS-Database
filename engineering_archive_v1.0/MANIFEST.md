# AstroCS 自治工程控制包清单

## 压缩包根目录

- `AUTONOMOUS_START_PROMPT.txt`：用户唯一需要发送给 Agent 的启动提示词；
- `BOOTSTRAP_README.md`：零手工部署说明；
- `bootstrap/install_control_pack.py`：跨平台安全安装/恢复脚本；
- `bootstrap/Install-AstroCS-ControlPack.ps1`：PowerShell 入口；
- `engineering/`：完整工程控制中枢。

## engineering 目录

- `00–15`：项目章程、基线审计、目标架构、工程控制、数据、接口、构建、测试、Stage 验证、可观测性、发布和路线图；
- `control/`：项目状态、当前任务、任务依赖、需求、接口、数据集、风险、决策和自治策略；
- `agent/`：自治主 Agent、复核协议、恢复规则和受控执行规则；
- `tasks/`：P00–P08 分阶段任务说明；
- `checklists/`：任务入口、代码、数据、接口、Stage 和发布验收清单；
- `templates/`：任务、测试、证据、复核、阻塞、检查点、缺陷、ADR、数据集、接口和发布模板；
- `tools/`：仓库只读预检、控制包校验和任务状态控制器；
- `evidence/`：后续任务证据目录。

## 初始状态

- 当前任务：`P00-001`；
- 任务总数：62；
- 自动执行方式：单主任务串行、独立复核、复核通过后自动推进；
- 停止条件：真实硬阻塞、运行环境强制结束或全部任务完成。

## 自治启动入口

压缩包根目录的 `AUTONOMOUS_ENTRY.md` 是极简启动词对应的完整执行入口。
