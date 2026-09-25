# 核验-CIT-01 · 引用文献核验原始取证

- 被审基线：`c8f64e9a`
- 批次：CIT-01（18 条未核引用，P-1）
- 清单来源：`独立审计/批次清单/CIT-01.txt`
- 判据：`独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md` §1 三字段（存在性／关联性／版本）
- 网络手段（本机 shell 无外网，一律走联网工具）：
  - arXiv 号 → `https://export.arxiv.org/api/query?id_list=<id>`（官方 API，回包含版本号）
  - DOI → `https://api.crossref.org/works/<doi>`
  - DOI 兜底 → `https://doi.org/<doi>` 解析后的出版社页
  - ASP Conf. Ser. 会议论文 → `https://aspbooks.org/custom/publications/paper/<卷号>-<页码>.html`
  - 图书／专著 → 出版社页或馆藏记录
- 仓库侧：只读（`git grep -n` / `Read`），未跑构建与门禁，未改任何仓库文件；共享台账 `整改/out/文献台账.csv` 未写回。
- 每条取证上限：3 次网络查询 + 2 次仓库读取；到限未决即记 `UNPROVEN` 并附用过的标识符与途径。

## 19 · DOI 10.1080/03610927708827533（命中 6 次，PLATESOLVE 等） —— 核验态：未核

## 69 · DOI 10.1017/S0252921100007685（f-instr-survey） —— 核验态：未核

## 89 · arXiv:1003.5613（EXP-06-SNR-PHYS） —— 核验态：未核

## 95 · arXiv:1706.01542v1（REVERSE_VERIFY_BIBLIOGRAPHY） —— 核验态：未核

## 101 · arXiv:2301.05793（EXP-02-STRUCTURE-CONTAMINATION） —— 核验态：未核

## 107 · arXiv:astro-ph/9808087v2（DRIZZLE_GEOMETRY / science·DRIZZLE） —— 核验态：未核

## 113 · DOI 10.1086/117915（PHOTOMETRY） —— 核验态：未核

## 119 · DOI 10.1088/0004-637X/756/2/158（snr-propagation-design） —— 核验态：未核

## 125 · DOI 10.21105/joss.00058（JOSS） —— 核验态：未核

## 131 · DOI 10.6028/jres.070c.025（NIST JRes） —— 核验态：未核

## 137 · 2010ASPC..442..435B（SCIENTIFIC_REFERENCES） —— 核验态：未核

## 143 · arXiv:1008.0815（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE） —— 核验态：未核

## 149 · arXiv:1612.05245（REVERSE_VERIFY_BIBLIOGRAPHY） —— 核验态：未核

## 155 · arXiv:2101.02242（frame-snr-survey） —— 核验态：未核

## 161 · arXiv:2309.11533（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE） —— 核验态：未核

## 167 · arXiv:2608.17922（PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE） —— 核验态：未核

## 173 · DOI 10.1007/978-3-662-26811-7_58。（PHASE2_SAMPLER） —— 核验态：未核

## 179 · DOI 10.1051/0004-6361/201322746（逐字复述，PHOTOMETRY_LIT_REVIEW） —— 核验态：未核

<!-- PROGRESS: 0/18 -->
