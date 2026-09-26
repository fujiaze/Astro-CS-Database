# 独立审计文献核验成稿 | CIT-20
**批次号:** CIT-20
**基线:** c8f64e9a
**网络自检状态:** 200 (arXiv API OK)
**核验者:** Qoder (CIT-A 文献核验执行路)

---

## 275 · DSS Merged Norder3 HIPS URL —— 核验态：已核
**存在性:** 已核 (HTTP 200，content-type: application/fits, content-length: 527168)
**关联性:** 关联对 (文献 [S18] 实测取样 1 直接使用此 URL 验证 HiPS tile 头结构不含 HEALPix 强制关键字)
**版本:** 版本对 (当前线上版本即为文献所引版本)
<!-- PROGRESS: 1/6 -->

## 276 · 2MASS J HIPS URL —— 核验态：已核
**存在性:** 已核 (HTTP 200，content-type: application/fits, content-length: 1054080)
**关联性:** 关联对 (文献 [S18] 实测取样 2 直接使用此 URL 验证 HiPS tile 头含 ORDER/NPIX 等非强制扩展关键字)
**版本:** 版本对 (当前线上版本即为文献所引版本)
<!-- PROGRESS: 2/6 -->

## 277 · Gaia/COSMOS ESDC HIPS URL —— 核验态：不存在
**存在性:** 不存在 (两次 HTTP 请求均返回 404；网关重试后仍 404)
**关联性:** 关联错 (位点断言暗示该 URL 可访问作为参考源，但实际不可达；需改称"曾存在或路径变更")
**版本:** 未钉版次 (URL 不存在故无法核对版次)
<!-- PROGRESS: 3/6 -->

## 278 · COSMOS ESA Archive HIPS URL —— 核验态：不存在
**存在性:** 不存在 (两次 HTTP 请求均返回 404；网关重试后仍 404)
**关联性:** 关联错 (位点断言暗示该 URL 可访问作为参考源，但实际不可达；需改称"曾存在或路径变更")
**版本:** 未钉版次 (URL 不存在故无法核对版次)
<!-- PROGRESS: 3/6 -->

## 279 · COSMOS ESA AC HIPS+MOCS URL —— 核验态：不存在
**存在性:** 不存在 (两次 HTTP 请求均返回 404；网关重试后仍 404)
**关联性:** 关联错 (位点断言暗示该 URL 可访问作为参考源，但实际不可达；需改称"曾存在或路径变更")
**版本:** 未钉版次 (URL 不存在故无法核对版次)
<!-- PROGRESS: 3/6 -->

## 280 · COSMOS ESA Ask Esky HIPS URL —— 核验态：不存在
**存在性:** 不存在 (两次 HTTP 请求均返回 404；网关重试后仍 404)
**关联性:** 关联错 (位点断言暗示该 URL 可访问作为参考源，但实际不可达；需改称"曾存在或路径变更")
**版本:** 未钉版次 (URL 不存在故无法核对版次)
<!-- PROGRESS: 3/6 -->

---

## 文末四节（按派单前言 §5）

### 本批三态计数

| 维度 | 已核 | 不存在 | UNPROVEN |
|---|---|---|---|
| **存在性** | 2 (275, 276) | 4 (277–280) | 0 |
| **关联性** | 2 (275, 276 对) | 4 (277–280 错) | 0 |
| **版本** | 2 (275, 276 对) | 4 (URL 不存在) | 0 |

### UNPROVEN 清单

本批无 UNPROVEN 条目（所有标识符均已通过 HTTP GET HEAD 途径完成核查）。

| 序号 | 标识符类型 | 标识符值 | 途径 | 结果 |
|---|---|---|---|---|
| 275 | URL | https://alasky.cds.unistra.fr/DSS/DSS2Merged/... | curl -sS -I | 200 OK |
| 276 | URL | https://alasky.cds.unistra.fr/2MASS/J/... | curl -sS -I | 200 OK |
| 277 | URL | https://www.cosmos.esa.int/web/esdc/hips | curl -sS -I (×2) | 404 |
| 278 | URL | https://www.cosmos.esa.int/web/esac-science-archive/hips | curl -sS -I (×2) | 404 |
| 279 | URL | https://www.cosmos.esa.int/web/esac-science-archive/hips-and-mocs | curl -sS -I (×2) | 404 |
| 280 | URL | https://www.cosmos.esa.int/web/esasky/hips | curl -sS -I (×2) | 404 |

### 新发现的缺陷形态

**URL 失效但未同步更新文档（ESAC HiPS 路径变更）**：
- 277–280 共 4 个 COSMOS/ESAC 相关 URL 全部返回 404，表明这些页面已下架或路径迁移。
- 这属于**旧缺陷再暴露形态**：在 IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md "未能核实"节中已记录相同的 404 事实（第 554–561 行），但当时结论是"未能核实 ESAC HiPS 官方文档"而非直接判定"引用失效"。本次作为正式引用件核验时，应给出明确的"**不存在 / 关联错**"判据，而非模糊的"未能核实"。
- **改进建议**：文献台账中应对 URL 类引用增加"最后验证时间戳"字段，定期重测；对返回 404 的 URL，要么改称"历史参考（路径已变更）"，要么移除以免误导读者以为当前可访问。

### 覆盖率自报

**已核条数:** 6  
**分配条数:** 6  
**完成率:** 100% (6/6)

*以上结论基于网络实测与仓库代表位点 [S18] 文本对照；未见需要提交前台裁决的科学歧义或架构性冲突。*
