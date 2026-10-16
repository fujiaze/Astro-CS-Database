# 文档地图与工作边界

## 五篇原位修订主稿

- `实验/photometric-magnitude/REPORT_paper.md` — P1 Gaia XP 逐帧光度与独立误差预算
- `实验/absolute-snr/REPORT_paper.md` — P2 条件 SNR/协方差与真实星 coverage
- `实验/healpix-polar/REPORT_paper.md` — P3 球面 drop/叶边界完备性与数值验证
- `实验/dense-snr-reconstruct/REPORT_paper.md` — P4 稀疏 SNR、正值空间重建与支持域
- `实验/additive-sky-seamless/REPORT_paper.md` — P5 加性/乘性、图连通分量与星云保真

同名目录中的 `REPORT_experiment.md` 是**历史报告（文件开头已新增说明）**。必须通过新实验采样和本次编译指纹把历史读数升级成现时数据；用户原来的过往代码与记录没有被当作新实验凭空抹除，Git 历史可完整追溯论文旧稿。

## 共用

- `实验/README.md`
- `实验/shared/FIVE_PAPER_EXPERIMENT_PLAN.md`
- `实验/shared/references/CORE_REFERENCES_VERIFIED.md`
- `实验/shared/references/CORE_REFERENCES_VERIFIED.bib`
- `实验/shared/references/PRIOR_ART_CLAIM_MATRIX.md`
- `实验/engineering-evidence/acsd-science-docs-p5/DEEPSEEK_SCIENCE_EXECUTION.md`
- `实验/engineering-evidence/acsd-science-docs-p5/REVIEW_DECISIONS.md`
- `实验/engineering-evidence/acsd-science-docs-p5/HANDOFF_REPORT.md`

## 科学正本（均已完成部分纠偏，当前原则应继承）

- `docs/ACSD_DESIGN.md`: P3 理想数学 vs 数值误差；P4 参考场 SNR 与 scene SNR 分立；P5 加/乘差模必须有独立观测并保护低频结构
- `docs/science/drizzle/DRIZZLE.md`: 6.6e-12 历史位置集不等于全域守恒验证，明确源 drop 局部完备性与总通量的区别
- `docs/science/sky/UPM.md`: 每连通分量 gauge、共模不可恢复与默认恒等增益、接缝表示域及检测功效

## 不在本轮文档修订“已完成”的事项

整仓 112 条旧路径清理、P1–P5 数值重跑、真实 FITS 资产整理、生产 C++ 补丁、完整 CI/跨平台构建等仍是服务器工作。不要将这轮“修订五篇科学论文本身”误写成“已完成并验证五篇全部实验”。
