# ALG-002 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ALG-002 行(WCS PSF Photometry 离散算法, 对应 SCI-002 三合同); docs/algorithms/{PLATESOLVE,STAR_PSF_ALGORITHMS,PHOTOMETRIC_FIT}.md(T201/T202/T203 DERIVED); ALG-001 建立的 V5 合规模式。

## 动作
1. 三份文档逐份 V5 合规修复:
   - PLATESOLVE(ALG-WCS-001..002): 删 GPU 划分→CPU-only(帧级 worker pool 无硬编码); 新增 5c SIMD 安全(CD/SIP 逐元素独立+7×7 最小二乘固定序归约禁重结合+FP64 禁 fast-math)+取消点(帧/星表行块粒度)。
   - STAR_PSF_ALGORITHMS(ALG-STARPSF-001): 删 GPU 星批 kernel→CPU-only 逐星 LM(worker pool 星批并行无跨星归约); 5c(窗口 residual SIMD 安全+normal-equation 星内固定序归约)+取消点(星批粒度)。
   - PHOTOMETRIC_FIT(ALG-PHOT-001..002): 删 GPU 排序→CPU-only(IRLS 全样本固定序归约天然确定性); 5c(r_i 逐星 SIMD 安全+加权均值固定样本序归约)+取消点(迭代间, 未收敛不写 location/scale)。
2. ID 规范化+claim ID 格式修复: ALG-STAR-PSF-* 含连字符违反 claim_id 格式 [A-Z0-9]{2,8} → 全局改名 ALG-STARPSF-*(同步 PSF.md 下游与 modules/*.md, 4 文件)。
3. 三份 ID 行加 "> ID:" + "范围:" + V5 重验戳; 推导来源声明与 ALG-001 同式。

## 验证
- SCIENCE_CONTRACT_LINT_PASS kind=alg files=3 sections=10; kind=sci files=3 sections=15。
- 全量回归 unittest 21/21 OK(含 num_threads/GPU 词法 mutation)。

## 产物
docs/algorithms/{PLATESOLVE,STAR_PSF_ALGORITHMS,PHOTOMETRIC_FIT}.md; docs/science/PSF.md+docs/modules/*.md(ID 同步); 本日志。

## PASS 判定
三域(WCS/PSF/Photometry)离散算法均: 从 SCI 方程推导(既有 F1..Fn 保留)+误差模型(容差来源节)+复杂度+SIMD 安全+取消点+CPU-only 无硬编码; claim ID 全格式合规。ALG-002 = PASS。
