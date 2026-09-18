# per-doc: docs/science/PSF_SIGNAL_WEIGHT.md（SCI-PSFW-001）

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-H-01 | 43 | PSFSNR 式[18] c3(Σf)²/(c4σ_n²) | .pidoc 02（commit 08b8eb85）逐字核验 | CORRECT | — |
| R2-H-03 | 45 | 文章版 c1..c4；PCL 2.10.4 c1/c3 | .pidoc 式[17]/[19] 与 PCL master 2.10.8 逐行核验；版本标签不精确 | CORRECT / 标签不精确 | DESIGN-OK-DOC-WRONG |
| R2-H-04 | 42,47 | 官方式[16] 与 AstroCS 复合式披露 | 代码 psfsw.cpp:313-315、psfsw.h:57-61 逐行核验，披露准确 | CORRECT | — |
| R2-H-05 | 47（PENDING_OWNER_SIGNOFF） | α=2,γ=2、N=星间散度无 L1 标定 | 代码自标；无标定记录 | INSUFFICIENT-EVIDENCE | DESIGN-OK-EVIDENCE-MISSING |
| R2-A-02 | 117（UNRESOLVED 注） | 与 CONTROL_WEIGHT_SNR §2a 的 frame_snr 语义冲突 | 已登记 | 已登记 | DESIGN-OK-DOC-WRONG |

## 处置建议

- 保持 §3 的"受启发非等价"披露；补 L1 合成标定记录后冻结 α/γ 与阈值，或改回式(16) 结构。
- 常数引用带真实版本（PCL master 2.10.8 / GitLab commit）。
