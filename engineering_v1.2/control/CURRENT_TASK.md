# 当前任务

`P11-002`：建立标准 WCS 真实星对闭环诊断工具。

## P11-001 已完成（2026-07-27）

- 1 个冻结文档交付物：
  - COORDINATE_CONVENTION.md（13229 bytes，10 章 + 附录）
    - 7 个坐标系统定义（S1 detector / S2 U / S3 FITS WCS / S4 切平面 / S5 天球 / S6 HEALPix / S7 浏览器笛卡尔）
    - 7 个关键转换函数（S1→S2 / S2→S4 / S5→S4 / S4→S5 / S2→S3 / S3→S5 / S5→S3）
    - 22 个冻结变量（cx/cy/CRPIX/CD/SIP/shape/has_wcs 等）
    - Y 轴反转链（输入侧 det→U + 输出侧 U→FITS WCS）
    - 四模块一致性表（PlateSolve/Photometric/SNR/Drizzle）
    - 球面浏览器独立坐标系
    - Gaia 客户端约定
    - 禁止事项（不得先改符号 / 不得只在 Photometric 补偿 / 不得用旧路径替代闭环验证）
    - 变更控制流程（ADR + P11-002~P11-005 闭环验证）
- 1 个验证脚本：
  - verify_convention.py（19 项验证测试：contract 7 + unit 6 + consistency 4 + forbidden 1 + deliverable 1）
- 关键冻结约定：
  - CRPIX 1-based（FITS 标准），公式 width/2.0 + 0.5
  - CD 矩阵标准 WCS（无独立 1/cos(Dec) 因子），消费方不显式乘 cos(Dec)
  - SIP 索引 A[i*6+j] 对应 dx^i*dy^j
  - Y-flip 符号：A *= (-1)^j, B *= -(-1)^j, AP 同 A, BP 同 B, CRVAL/CRPIX 不变
  - 图像 shape=(height, width)，NAXIS1=width, NAXIS2=height
  - has_wcs: CTYPE 非空 + CD |val|>1e-15
- 19/19 测试 PASS
- 禁止捷径 PASS（无代码修改、无先改符号、无 Photometric 内补偿）
- 证据：engineering_v1.2/evidence/P11-001/

## 历史任务（已完成）

- P09-001：v1.1 基线冻结 + v1.2 开发包安装（4 件套）
- P09-002：INTERNAL_DETECTION_SHARED_EXPORT 命名统一（6/6 PASS）
- P09-003：canonical_dataset_v1.2 冻结（44 文件 SHA-256，7 测光失败帧，4 HCSD 基线）
- P10-001：TestData 目录盘点（3 设备 / 49 light 组 / 49+27 Header 采样）
- P10-002：T1-T4 设备档案建立（4 profile + summary，710 lights，76/76 PASS）
- P10-003：主校准帧盘点（27 文件 CSV + summary，20/20 PASS）
- P10-004：滤镜规范名与别名冻结（52 别名映射，23/23 PASS）
- P10-005：Light 到 Master 唯一解析（587/710 resolved，123 missing_lum_flat，23/23 PASS）
- P10-006：T1-T4 真实校准代表帧验证（16/16 PASS，25/25 测试 PASS）
- P11-001：坐标约定冻结（COORDINATE_CONVENTION.md，7 坐标系 + 22 变量，19/19 PASS）

## 下一步：P11-002

依据 `tasks/P11-002.md`（待查阅）：

- 建立标准 WCS 真实星对闭环诊断工具
- 依赖：P11-001（已满足）+ P09-003（已满足）
- 基于冻结的坐标约定构建诊断工具，验证 WCS 可回投真实星点

## 已知 BLOCKED 项

- 123 Lum Light 帧（T2 25 + T4 98）缺 Lum Flat Master，需用户决策（提供 Lum Flat 或批准 flat-skip）。已在 P10-002 设备档案中记录。
