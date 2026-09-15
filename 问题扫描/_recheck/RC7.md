# RC7 复核档案 · recheck_round1 verify[6::8]（61 条）

- 时点：HEAD `a3a343a44080d917089e1f8d548ed2d0400b0c61`（BASE `521095b8`）
- 分片：`问题扫描/_cache/recheck_round1.json` → `verify[6::8]`（i=6），实测 61 条（494 条取模第 7 组）；不触碰他人区间
- 方法：一律按 文件::符号 重新定位（glob/grep/read + 只读 python3 统计 + `git --no-optional-locks diff/show/log` 只读），复算原缺陷机制是否消失；缺失判定走三级复核（①工作树 ②HEAD `git ls-files`/`git show HEAD:<path>` ③改名/移动检索）
- 工作树状态：脏改含 `docs/TRACEABILITY.csv`、`artifacts/prerelease_v5/ISA-00{1,2,3}/MEASUREMENTS.csv`、`reports/v19r2|v19r3/*`、`设计大纲/**`；涉及时单独注明，结论以 HEAD 入库态为准
- 四态口径：STILL（缺陷仍在，给新锚）/ FIXED（给"修在哪"证据：文件::符号 + 关键 diff 或新逻辑，并判是否修全/同类他站是否仍在）/ MOVED（位置变缺陷原样）/ CANNOT_STATIC（需运行期）
- 本档只输出**建议标记**，不回写账本、不 commit（账本由前台统一处理）

## 一、结论总表（收工回填）

| # | ID | 优先 | 类别 | 四态 | 新锚（文件::符号） | 一句话判据 |
|---|----|------|------|------|--------------------|------------|

## 二、逐条复核记录

<!-- 逐条追加 -->

## 三、统计（收工回填）

- STILL: / FIXED: / MOVED: / CANNOT_STATIC:
- 建议账本标记（仅建议，不回写）:
