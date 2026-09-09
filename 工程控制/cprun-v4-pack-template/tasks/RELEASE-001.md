# RELEASE-001 发布窗口任务（external 门禁演示）

## 目标
在 Windows 环境执行平台相关验证（示例：打包冒烟）。

## 输入
- 修复后的工作区（上游 FIX-001 / CI-TEST）

## 要做的事
1. 执行 Windows 冒烟脚本
2. 结论回填证据报告

## 验收条件
- smoke：冒烟脚本退出码 0

## 说明
本节点被 external 门禁 `CI-GATE` 控制：外部 CI 状态翻转为 success 前保持 pending；
前台可用 cp review 的 externals 字段翻转外部状态。
