# 执行 ACSD 阶段一完整收敛

仓库：`fujiaze/Astro-CS-Database`
分支：`audit/acsd-science-docs-p5`

先 fetch 并核对远端最新历史、工作区与本文件；保留已有修改，不强制覆盖。代码起点包含 `950c261be`，还必须包含前台最新独立复核、正式数学/接口补充和完整任务包，不能从旧包继续。

本轮的完整执行依据：

- `实验/normalize-precision-p5/NORMALIZE_COMPLETE_TASK_PACKAGE.md`
- `实验/normalize-precision-p5/INDEPENDENT_REVIEW_950C261.md`

先读 AGENTS.md 和 docs/ACSD_DESIGN.md，再按任务包顺序完整阅读 science 正本、NORMALIZE_PIPELINE、NORMALIZE_DATA、docs/detail/normalize/contracts.md 与相关 API/模块页。特别阅读新增的异方差 GLS、最终 signal 算子、gain/alpha、空间积分 PSF、参考 SED 与 Runtime/reader 数据边界。任务包不替代正式定义。

目标是一口气完成整个 Normalize 八节点的代码、数据合同、产品和科学验证收敛，不限于修复 F01–F08。按任务包 A–G 的依赖持续实施：先修精度数学和真实信号 R，再完成 Runtime typed 对象逐边迁移、校准/坏点/IPV/空间 PSF、逐帧 XP 响应与两种参考定义、噪声与可重算模型、唯一 writer/完整 reader、几何/层级/开发模式验证，最后统一实际 API/机器合同与事实报告。

保留成熟算法并在对应算法库修复，scheduler 不复制数学。IPV 继续作为唯一自带解算器；p1_session 暂留兼容 facade。普通节点取数改为真实 Runtime 对象，大面 spill/staging 复用 AIO。缺设备参数的单帧可以明确 approximate/unavailable，但有完整输入仍 not_wired 或仅记录字段未真正消费，不算完成。

使用解析真值、独立数值/随机模拟和 `/testdata` 真实输入，保存输入清单/哈希、命令、原始读数、失败案例和实际预览。先复现再修复异方差 GLS 反例，不改 oracle 或放宽容差凑全绿。真实无头WCS、测光标度生效、多帧/混合设备与滤镜、模型落盘后独立重算、OFF/ON及资源条件对照均需覆盖；环境或数据确实受限的项目精确报告，不冒称验证完成。

不改 Mosaic 默认权重、背景或 Export 算法，不上传原始 FITS/母版/HST 大文件。所有实现、脚本、小型证据和最终事实报告直接提交推送当前分支，不建 PR、不改 main、不 force-push。按依赖分批提交，不用每批等待批准，不以“首批完成”提前结束整个任务。

基础科学/架构正本由前台维护；真正未定义或被独立反例否定的问题，提交具体条款、源码与证据给前台，并继续不依赖该问题的工作。完成全部任务后报告每项实现位置、生产消费证据、误差、产物、SHA和仍未完成事项，供前台独立复核。现在开始执行。
