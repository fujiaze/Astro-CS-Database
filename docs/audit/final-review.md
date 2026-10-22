# 修订后的最终独立复核

实际规范、声明、schema、例子、检查器输入见[final-input-hashes](final-input-hashes.json)。报告是三位审核 Agent 返回的结构化记录；审核者均未参与编写、未修改文件，未以旧代码或旧测试为依据。科学审核侧重数学/数值与误差，合同审核侧重公开载荷可调用/可重开，全新审核者从陌生开发者视角追踪全链且不读既有审核报告。

## 最后一项失败及再次复核

合同审核者 /root/independent_contract_review 与全新审核者 /root/fresh_completeness_review 独立发现同一P1：contracts.hpp 的 StarFlux/SourceMeasurements只有名义counts和随机Variance，但EXECUTION要求counts数值区间进入photometry score。固定精确PSF，同nominal=100、同随机noise，Native区间[100,100]与[99,101]，q=100精确；旧返回可逐字段相同，正确a却是1与[.99,1.01]。fit_photometry不接隐式Native，无法恢复差别。

最小修订已落实：SourceMeasurements新增与stars同序的flux_numerical_lower/upper、强引用error和observation；OBJECTS最后一节唯一规定单位/Index/缺界；SYSTEM最后一节规定输入与模板区间经验证求解/score根形成实际通量界、证书归并并强引用、photometry消费而不补零；VERIFICATION新增上述反例。PsfEvaluationRequest.output_rows支持实际窗口/依赖块读取，省略行不重归一，避免小窗口强制全图×源数模板。审核指出一个非阻碍错误码拼写invalid_argument，已统一实际Code::invalid_request。

再次独立复核结果：

- 合同审核：剩余R2关闭，未发现新P0/P1；measure→photometry→α的实际字段连接闭合，后者区间[100/101,100/99]不能被nominal/covariance掩盖；依赖图为前置Native→测量→PhotoSolution→新标度版本，没有反向边。完整Normalize→持久化→Mosaic→Export有可追踪载荷。
- 全新审核：此前context、U01别名、Storage枚举三项已关闭；最后counts连接和output_rows也闭合，未构造新P0/P1；可以交给不懂旧ACSD的编码Agent按规范开始完整实现。
- 科学审核 /root/independent_science_review：此前S1–S6及PSF误差P1关闭，未发现新的科学/核心接口阻碍。实际核对signed T=[1,-1]误差为2ε而非0；a=2、α=.5模板恢复幅值；空间a=4、p=(.25,.75)、α=(.5,.25)得到(.5,.75)且不能重归一；bin沿G Dα；flat响应与参数数值盒独立于随机covariance；固定WCS只能条件已声明模型。

合同/全新审核实际重跑302 definitions、77 operation variants及四份请求闭包检查；独立解码24byte小端块为(2,4,8)，给定R得(3,7)；U01独立矩阵真值277/64。三者没有运行尚不存在的科学实现。

## 结论与界限

**设计完整性通过，READY_FOR_OWNER_REVIEW；未发现仍阻碍实施的P0/P1。** 独立审核的判断是：不了解旧ACSD的编码Agent可以仅按本分支公开科学与工程规范开始完整实现，不需另作关键架构决策。正式负责人设计批准仍待进行。

本结论不是算法/生产验收。幅值、signed误差、partial重开、几何细化、秩/可估性、大数据资源与Windows/Linux完整程序必须在实现后按VERIFICATION用独立真值实跑。当前只有声明编译和设计完整性实测，不能以此声称科学功能已经通过。
