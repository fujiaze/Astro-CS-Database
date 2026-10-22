# 第二轮独立审核：仍有具体编码阻碍

输入：commit 11ae71c20ad8f692cfeb0b24a0a5143765869402；[冻结hash](round2-input-hashes.json)。下列是审核返回的结构化摘录，保留未通过结论，非算法执行报告。审核者不修改设计。

| ID/审核者/级别 | 位置与失败构型、影响 | 最小修订与最终处理 |
|---|---|---|
| R1 合同/新审核 P2 | examples/u01.request.json，CSR行/nnz数组引用四个未提供Index；公开driver不能解码 | 提供F_csrrow/F_csrnnz/R_csrrow/R_csrnnz，检查全部实际别名 |
| R2 合同/科学 P1 | evaluate_psf返回裸Array，signed transfer求积/参数误差在information及photometry处失去实际证书；请求tolerance不是结果误差 | PsfEvaluation含values、数值lower/upper、tail、双轴Index、ErrorRecord；InformationRequest绑定evaluation+column，后续仍需源通量载体，见最终报告 |
| R3 合同 P1 | schema/driver仅inline Array，native数组/几何dyadic块无法仅凭公开请求进入；大输入必须用私有会话 | BlockDescriptor、blocks/input_root、Storage.open，读出块可再次输入；真实24byte块示例和hash/size/alias检查 |
| R4 合同/全新审核 P1 | Native无ObservationContext，2016与2026同像元观测失去历元/observer，盲解无法传播Gaia；frame与天测解来源也弱 | Native.context/frame逐版本保留；FootprintSet.astrometry与bind_wcs solution强引用；固定WCS条件误差明确 |
| R5 全新审核 P2 | Storage.copy/read与product.read_array未进入独立driver operation枚举 | 生成器纳入这些具名方法；schema由唯一声明同步 |

合同审核确认原C1–C6、C8在此时已关闭，C7经实际schema检查关闭；科学审核确认原S1–S5关闭，S6数学PSF定义已闭合但实际求值误差尚缺载体。第二轮不能据文件数量或schema存在宣告完成。

之后还保留了PSF源端幅值a与像元端α、sample_response_gain、物理bin先Dα后G、固定天测条件的具体反例及修订。所有最终结论见final-review，不将中间失败回写为通过。
