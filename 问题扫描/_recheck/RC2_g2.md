# RC2 复验档案 · 第 2 片（g2，verify[i::8] i=1，62 条）

- 时点：HEAD `a3a343a44080d917089e1f8d548ed2d0400b0c61`（工作树对源码区与 HEAD 一致，仅 设计大纲/artifacts/evidence/reports 49 个非真源文件脏）
- 分片来源：`问题扫描/_cache/recheck_round1.json` → verify[1::8]（62 条），ID 清单快照存 `问题扫描/_recheck/_g2_ids.json`
- 方法：每条先读定稿原文（存 `问题扫描/_recheck/_work/<ID>.md`），再按 文件::符号 重定位（不依赖旧行号），复算缺陷机制是否仍在；git 侧仅用 --no-optional-locks 只读命令
- 四态：STILL（缺陷仍在+新锚）/ FIXED（已修+修在哪证据+是否修全）/ MOVED（位置变缺陷原样）/ CANNOT_STATIC（需运行期）

## 结论速览（收档时填全）

| # | ID | 四态 | 新锚（文件::符号/行） | 一句话依据 |
|---|----|------|----------------------|-----------|
| (待填) |

## 逐条明细（待填）
