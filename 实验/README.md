# 实验

本目录是科学实验单元的家：每个单元围绕一个科学命题自包含地给出假说、方法、数据、结果与论文式报告，可独立复核。

## 职责边界

- 放：随仓库维护的实验单元——固定 seed 的实验代码（code/）、输入数据（data/）、结果与判据表（results/）、支撑推导（docs/）、文献台账（refs.md）、REPORT_experiment.md 与 REPORT_paper.md。
- 放：工程实测类证据留档（engineering-evidence/）——测量结果 / 基准数值 / 质量实测 / 审计时点证据，按负责人裁决（2026-09-28，artifacts 重组）自 artifacts/{evidence,acceptance} 迁入留档，历史记录不改写。
- 不放：科学公式正本与推导（docs/science/、docs/science/algorithms/）；单元报告只引用这些权威，不复制正文。
- 不放：一次性运行产物与日志（run/）、CI 运行产物（artifacts/ci/）、机器门基线与台账（artifacts/evidence/）。
- 自包含要求：仅凭一个单元目录即可回答假说、方法、数据、结果、诚实边界、复现命令与佐证来源，不依赖单元之外的临时文件。

## 内容

- photometric-magnitude/ —— 创新点一：测光校准到星等坐标系的实验单元。
- absolute-snr/ —— 创新点二：跨帧绝对信噪比的实验单元。
- healpix-polar/ —— 创新点三：平面到球面守恒映射与极区面积交叠的实验单元。
- dense-snr-reconstruct/ —— 创新点四：由稀疏控制点重建稠密信噪比场的实验单元。
- additive-sky-seamless/ —— 创新点五：加性天光去除与无接缝叠加的实验单元。
- cone-search-constants/ —— 锥形搜索四项常数的论文式精读报告单元。
- m42-realdata/ —— M42 真实数据腿单元，为创新点提供第三类数据佐证。
- shared/ —— 各单元共用的合成数据生成器（synthetic/）、共用数据（data/）与参考文献（references/）。
- engineering-evidence/ —— 工程实测证据留档（ISA 性能实测、质量审计、压缩评估、L2 性能门读数、发布验收证据等；自 artifacts/{evidence,acceptance} 迁入，归属口径见其 README）。

## 上游

上游：docs/ASTROCS_DESIGN.md §12.1（科学正确性与三重佐证）、§12.2（三类实验数据）、§12.3（创新点实验单元）。

科学断言以 docs/science/ 为权威，佐证要求见 docs/DOCUMENT_GOVERNANCE.md §2。