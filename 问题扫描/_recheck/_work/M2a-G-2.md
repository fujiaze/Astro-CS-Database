## M2a-G-2 DATA 合同 ID 闭包检查器的正则只覆盖带 -NNN 后缀的形态，约半数在册 DATA-* 落在门外，而合同自称「全部登记、无重复」

- 类别: G_GOV_GATE
- 优先级: P2
- 来源: L10-021
- 位置: tools/check_data_artifacts.py::DATA_RE 与扫描/比对段；docs/contracts/DATA_ARTIFACTS.md::§3 机器校验行 + ID 索引表；docs/contracts/DATA_SEMANTICS.md::DATA-* 标题面；docs/contracts/SCIENCE_PROFILES.md、docs/traceability/TRACEABILITY_MATRIX.md
- 证据摘录（逐字，复核时点现文）:
  > （tools/check_data_artifacts.py:16-17）DATA_RE = re.compile(r"^DATA-[A-Z0-9-]+-\\d{3}$")   // 仅匹配三位数字后缀
  > （DATA_ARTIFACTS.md:102-103）- `tools/check_data_artifacts.py`（DATA-001 新增）：校验本表 schema_id 唯一、DATA-SEMANTICS.md 中声明的 DATA-* ID 全部在本表登记、无重复。
  > （DATA_SEMANTICS.md 标题面）形如「### 5.1 DATA-P1-CAL …」「… DATA-P1-STACK …」的登记项**无** -NNN 后缀，故不被 DATA_RE 采集
- 权威依据: 宪章 §12.3-1/-5（单一事实源与机器一致）、§17.2（追溯无断链）
- 问题说明: 检查器以正则枚举「声明的 DATA-* ID」再与登记表比对，但正则把不带数字后缀的命名形态整体排除（如 DATA-P1-CAL / DATA-P1-STACK），于是这些条目既不查重也不查缺；同表另一侧的 ID 索引存在同族 ID 双名并存，合同却写「全部登记、无重复」。该门给出「通过」信号时，其覆盖范围只有约一半。
- 影响: DATA 合同 ID 闭包的门禁效力被高估；新增 ID 若取无后缀形态可完全绕过检查。
- 建议处置: ① 放宽 DATA_RE 至 `^DATA-[A-Z0-9-]+(-\\d{3})?$` 或按登记表命名规范显式两式并采；② 消除同族双名（保留一个）；③ 在合同 §3 写明机器门实际覆盖形态。
- 置信度: 高（正则与两侧文本形态一手核对）
- related: M2a-C-10、M2a-C-11、M2a-G-1
