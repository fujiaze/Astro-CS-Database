# ARCH-AUDIT-P1｜P1-001 独立架构抽验（负责人管理意见）

## 背景（owner 管理意见 2026-09-09）
P1-001 前台验收 passed 仅代表任务验收结果，不代表 Phase1 产品可用。负责人要求对 P1-001 保留一次**独立架构抽验**（独立 SubAgent，非原实现者自查）。

## 抽验对象
commit 9e09941a（P1-001 三域真实化）+ BASE=9e09941a 工作树。

## 五项确认（逐项给独立证据）
1. **WCS 节点执行真实 Plate Solve**——ipv 求解器链真实调用（gaia 句柄、ipv_solve_from_memory_with_callback_d），不是仅消费配置 WCS 做 pix2sky；Linux stub fail-closed 行为符合平台合同（不冒充）。
2. **PSF 节点执行真实 PSF 拟合**——dpsf_fit_batch_f64 Moffat4 真实拟合，不是用星源特征（fwhm/ellipticity）代替。
3. **writer 生成标准化单帧 HiPS**——IVOA 1.4 标准 HiPS（signal/support/properties/MOC），不是 calibrated FITS 改名。
4. **module_adapters.cpp 没有成为新的巨型科学实现层**——节点薄适配、科学实现在对应模块库；检查行数/职责边界/是否内联了本应在模块库的科学代码。
5. **不完整 Phase1 链不能写 complete**——complete 门语义核实 + 故障注入某节点后下游 call_count 必须为零的用例真实存在且必败。

## 边界
- read-only：零代码写入（审查报告落 evidence）；发现缺陷登记 finding 移交，不修。
- 审查独立性：从 git 历史与当前代码独立取证，不采信 P1-001 agent 自述。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。

## 验收
- 五项逐项结论（PASS/FAIL/CONCERN + 证据定位）；
- 前台复验后才可 PASS；若有登记面修改，单独原子 commit。

## 返回证据
schema 要求 scope/acceptance/provenance + artifacts（逐项证据定位表：文件:行号、测试名、git show 片段）；不得伪造 PASS。
