# 文档重构：范围、处置与待办

本轮直接在 main 整理文档，科学与数学疑点留待后续统一推导。本报告记录实际改动和覆盖范围；**全仓逐篇实质重写尚未完成**，不能将清单生成、路径迁移或结构检查记为阅读全文、文献审核或科学验证。

## 范围与原文身份

基线为 `63eccbb6133aef56389641d43c55df6e243819c5`。从 Git 跟踪树建立[原始文件清单](docs/ORIGINAL_FILES.tsv)，覆盖 702 份技术文本，包括全部 191 个 docs 文件、246 份实验文本及根说明、源码说明、schema 和构建文本。实验集合比最初 231 份统计多出 .bib/.log 等文本；两个数的分母不同。

每行保存路径、Git blob、字节数、处置、内容归属和实际阅读范围。它是本轮集中处置记录，不建立审批状态。以原 blob 和基线可恢复原文，例如：

```sh
git show 63eccbb61:docs/detail/registry/acsd.phase1.drizzle.md
```

未跟踪的 wiki、归档目录、工作包以及 lib/healpix_db、lib/phase2 不纳入原始清单，也未移动或删除。用户粘贴方案保存在附件中，本报告不改写其完整交付要求。

## 已落实的结构

| 内容 | 归属与实际变化 |
|---|---|
| 根入口与协作 | 重写 README、AGENTS，统一 main 工作方式，修正不存在路径和过强科学能力描述 |
| 最高设计 | 按目标、三阶段、资源、I/O、GUI 和证据重写；公式和文献依据指向专题，不另立简化模型 |
| Normalize 科学链 | 按观测量、依赖、校准、WCS、PSF/光度、条件噪声、球面完成、产品及验证重写；保持参考、单位、GLS 和同次 R 定义 |
| 排异与集成 | 重写为候选资格、归约、六态、参数示例及后选择误差的连续说明；数值策略没有切换 |
| 公共 API | 全文读完来源清单并逐字归档，正式页收缩为接口规则、真实头/模块定位与关键实际声明；全函数实现验证未完成 |
| Runtime 与寿命 | 对照 runtime/module/context/artifact 及 CLI 实际代码，重写 Runtime、所有权合同与基础设施细节，明确调用方创建 ctx 和异步借用边界 |
| 测光文献阅读 | 保留 44 项书目，原研究注记完整归档，冲突模型陈述归问题记录，正式页区分阅读任务与核验结论 |
| 模块正文 | 25 份登记页的完整有效正文归入三个阶段 modules.md 和 infrastructure/hips_writer.md；按生产依赖排序，保留局部 session 与 CLI 的区别 |
| 文档维护 | DUAL_LINE、TRACEABILITY、DOCUMENTATION 并入 DOCUMENT_GOVERNANCE，统一协作、写法、追溯和索引 |
| 科学附录 | CCD 缺陷观察、天测预算、检测记录与 PSF 复算附录独立保留于本单元 docs；PSF 示例另存 code，未执行 |
| 目录导航与机器定位 | 修订 Markdown、源码 README/module.yaml 及 JSON 中的文档路径；JSON 结构、数值和科学配置不变 |
| 科学问题 | [23 项待推导/核验问题](docs/SCIENTIFIC_QUESTIONS.md)及 15 项工程/文档缺口，各列位置、影响和所需证据 |

旧到新的具体文件关系见[路径迁移](docs/PATH_MIGRATIONS.json)。当前 docs 为 167 个文件；减少 28 个原始页，增加 4 个模块正文。目录 README 是阅读入口，不承担第二份接口定义。

原始文档中保留的正式专题不因“文字看起来完整”而标为已验证。数学恒等式、参考通量条件、科学上界前提和用户自由组合政策未在本轮重新选择。生产源码、测试、构建和科学算法均未修改；新增代码只用于文档整理与结构检查，另保留未执行的复算示例。

## 已读与核对范围

完整阅读的主题包括最高设计、统一模型/数据语义/适用域、Normalize 科学链、校准/缺陷、天测/检测、PSF、光度、噪声、Drizzle、背景、排异、集成、投影/重采样、HiPS 格式，以及相关架构、收敛、问题和模块登记正文。原始清单逐行标明阅读或局部阅读范围。

实现抽查了 CLI 的 run_phase1_block 和三阶段入口位置、P1/P2/P3 节点分派、Normalize 公共类型、sidecar 公共结构、parse_reference_config、Drizzle precision staging、QR 信息相减与集成 reducer。模块原文中未逐函数重新核对的实现陈述保留其限制，不能因此称全部 API 已核实。

公共 API 清单全文已读，来源字节保存在[PUBLIC_API_SOURCE.txt](docs/PUBLIC_API_SOURCE.txt)，检查器与基线逐字比较。Runtime、Module、Context、Artifact 公开头及 CLI 的工厂/借用调用已经核对；它们不代表全部算法函数逐一实现验证。

校准算法页已读，其他超长算法资料、尚未处理的工程合同和实验树仍只部分查阅，不能记为全仓精读。外部论文与标准没有逐项重新联网阅读；文献目录也未完成统一核验。

## 待推导和实现风险

[问题记录](docs/SCIENTIFIC_QUESTIONS.md)区分数学定义、已有条件证明、仍缺误差证书和静态代码风险。主要待办包括跨响应的输出估计量、完整域空间标度、PSF 求积与拟合一致性、QR 秩及通量精度、参考到场景转换、稀疏支持、背景规范/交叉项、排异后选择以及曲边与导出误差。

静态抽查还发现：p1_op_drizzle 销毁 PipelineFrame 后，缺有效 EXPTIME 的分支仍可能读取该 frame 的 header。该路径已列为 E07，没有运行验证或代码修复。其余已定规格的迁移继续由[实施差异](../../docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)承担，不当作自由选择新模型的授权。

历史实验原文和故障注入脚本没有批量改写。它们可能引用退役路径，按基线 blob 或迁移表解释。配置 JSON 的旧 quote、章节和内容哈希也不会因文件路径更新自动获得正确性，问题记录 E09 明确这一限制。

## 检查与过程

可复跑结构检查：

```sh
python 实验/documentation-refactor/code/check_documentation.py
git diff --check
```

检查覆盖当前 docs、根入口及本单元新增 Markdown；历史实验不属于本轮修复链接集合。它验证 YAML 重复键/路径、索引与当前文件集、上游文件、模块正文完整保留、迁移后新增失效链接及 JSON 元数据变更。结果保存在[CHECK_RESULTS.json](docs/CHECK_RESULTS.json)，只能解释文档结构。

整理过程出现的失败保留如下：

- 第一次 apply_patch 同一路径同时 Delete/Add 被工具拒绝，没有写入；随后改为明确文件内容写入。
- 迁移脚本遇到 lib/algorithms/platesolve/cpp/ipv/make_clean_err.txt 的非 UTF-8 编码报 UnicodeDecodeError，后续明确跳过并保持原文件。
- 初次索引检查误把未跟踪 __pycache__ 当正式文档；改为基线 Git 跟踪集加明确新增文档，不删除缓存。
- 首次链接检查发现 REPORT.md 尚未生成的两处新增链接；补齐本报告后重新检查。
- 历史 doccheck 夹具被路径替换触及，已从基线逐字恢复；历史行证据不转写到新文件的旧行号。
- 检查结果文件首次生成前，报告已经链接它，触发新增链接错误；先写 NOT_RUN 标记再执行检查，成功结果由真实检查覆盖，避免把占位当通过。
- `[r_i-r_j](p)` 是显示公式的一部分，首版简易链接扫描将其误判为本地文件；扫描器明确排除代码与 LaTeX 数学域，保留真正文件链接检查。
- 一次批量 patch 因预期行不匹配整体拒绝，没有写入；API 全文读取后改为完整重写。一次工具存储键不可用发生在文件写入后，随后直接读文件确认写入状态，没有重复覆盖归档。

未执行构建、科学单元测试、数值推导、真实 FITS 管线或图像视觉验收。纯文档与路径整理不生成这些证据。

## 完整任务仍需完成的内容

剩余范围在清单中明确标为尚未精读或局部阅读：长篇算法资料的数学/工程拆分、其余工程合同的全文改写、模块的全函数/API 独立核验、实验文档逐篇归类、统一文献库核验，以及全体系再次穿行复读。PUBLIC_API 的正文拆分已落实，逐函数实现与并发验证仍在上述剩余范围。

本轮完成的是可审阅的入口重写、模块/规范归并、证据分离和问题集中记录。它没有完成用户方案中所有 191 篇逐篇六动作，也没有把问题登记当作科学推导或全仓实质重写的替代品。

## API 与 Runtime 续整理记录

全文检查 API 清单时，确认它混有 legacy orchestrator、局部 session 和真实 CLI 八/七/五节点的陈述，重复表格之间也不一致。重写后的正式文档通过真实模块和公开头定位有效接口，不把来源中的“全部真实存在”“bitwise”“未进根构建”等字样当当前科学事实。

Runtime 续查确认：run_pipeline 使用 RuntimeResourceBudget 工厂，在调用方栈上创建 RunContext，协作取消监视线程在同步 run 后 join；IModule 的公开成员不含 create/destroy，模块寿命来自工厂与 RAII。ThreadBudget 的三种获取政策和 ThreadLease 的真实预留/兼容大小租约分别说明。

截至本次整理，20 个原始文件已重写、28 个来源页已归并、4 个附录已分离。完整来源身份和逐文件范围以清单为准。没有修改 C/C++ 实现或运行科学程序。

## 日志与资源主题续整理

用户补充要求每次提交立即推送，已写入 AGENTS。第一次直连推送连接重置，第二次连接超时；随后发现本机可用 SOCKS 代理，成功 fetch。远端已推进到 `5b33e0417`，先合并其 Normalize Round 3 代码与实验记录，再将文档提交和合并结果以 `015a18b26` 推送 main。没有强推或重写远端历史。

此次新增全文重写6个原始文件：日志错误合同、错误模型、日志系统、可观测性、性能模型、资源监控。结构化日志修订来源权威与运行层接线说明；开发模式、探针标准、运行图全文已读，保留其专题内容。日志/资源旧正文完整保存于 OBSERVABILITY_SOURCE.txt 与 RESOURCE_RECORDS_SOURCE.txt，历史数字和相互冲突的评价规则没有丢失，也未变成新的运行结论。

静态核对覆盖核心 Error/Result/Cancel/ExitCode、CLI 实际域映射与失败 manifest 分类、probe.h/dev_diagnostics.h、CMake 开关、快照锁位置、monitor.py 的字段和指纹函数以及 trace_feed.py 完整 observer。新增 Q24 资源测量问题和 E16–E19 接线/身份/重复类型记录；本轮不改资源阈值或采样实现。原有 E01–E15 是合并前事实，按新代码逐项重核，不能用旧结论覆盖远端修复。

累计26个原始文件已全文重写，28份归并、4份附录分离，24个待推导/核验问题与19项工程记录。结构检查仍覆盖167个 docs、25份归并模块正文及15份仅导航字符串变化的 JSON。源码是合并远端而更新，文档整理自身没有修改 C/C++。尚未执行科学运行或宣称全部191篇完成。

## 构建与发行主题续整理

日志资源主题以 `2d0d442b5` 提交后立即成功推送。继续全文阅读 BUILD_GRAPH、BUILD_NODES、RELEASE、DEPENDENCY 和 ABI，核对根 preset、CMake 目标/链接声明、安装规则、图生成器、构建指纹声明源集、版本生成器与两平台打包入口。

重写前四份构建/依赖正文，原文按 blob/字节数完整保存于 BUILD_SOURCE.txt。过时的34目标手抄表、旧复现任务编号和未接线声明归档；正文改为当前构建关系、配置条件、工具局限及可执行入口。源码路径列表摘要、声明源内容摘要与实际二进制哈希分别解释，Windows CLI zip 和目标 Inno Setup GUI 安装包分别解释。新增 E20–E22，没有改编译器、指纹、打包器或产品算法。

累计30份原始文件全文重写；文档检查和源码阅读不代替构建、打包或实际科学验证。

## 数据、溯源与发布主题续整理

构建主题提交 `f23ce01e1` 后立即推送。继续全文阅读工件、产物库、溯源、阶段交换、原子发布、运行清单与统一工程对象，重写 ARTIFACTS、ARTIFACT_STORE、PROVENANCE、ATOMIC_PUBLISH 四篇，原文完整存入 PRODUCTS_SOURCE.txt。保留既有 DATA ID 的定位，删除旧表对模型、单位、未知误差和最终辅助通道的重复定义，统一引用科学正本和阶段交换政策。

实际阅读 Python Store 的 start/publish、HipsOutputStore::publish_directory 与覆盖恢复、provenance 版本解析/摘要声明、完整 atomic_publish.h。区分 Python 语义层与 C++ 生产接线；内容/清单/sidecar多次rename不称整体事务；发布可见性、完整性与持久化分别报告。新增 E23–E26，未执行 I/O 故障注入或更改发布代码。

累计34份原始文件全文重写。完整任务继续以 ORIGINAL_FILES.tsv 的实际范围推进，历史来源归档不作为余下文档精读的替代品。

## 空间索引、PSF与本地星表续整理

产品主题 `d21ec9b2c` 已立即推送。完整阅读公共 Gaia/噪声/诊断算法、HEALPIX_MAPPING、STAR_PSF_ALGORITHMS 和 Gaia detail，另核对完整共享HEALPix/Gaia头及PSF头、缓存比较实现。重写HEALPix数值说明、Moffat4算法说明、Gaia模块接口，PSF布局/调用细节并入已有 star-psf 模块段，原文完整存入 SPATIAL_NOTES_SOURCE.txt。

保留Moffat4公式、积分与残差系数，有限样本历史读数按证据归档；科学与数学疑点只记录，未换模型。新增Q25离散往返容差及E27–E29缓存/测试载体/状态布局问题。星检测逐算子长页只部分读到，未记全文完成。

累计37份原始文件全文重写、25个待推导问题、29项工程/文档记录；未完成的超长算法及其余正式和实验正文仍继续处理。

## 目录入口与工程标准续整理

空间主题 `e014863df` 已立即推送。阅读全文核对28份目录指南并改为对应主题的实际Markdown阅读路径，修正退役DOCUMENTATION指针、旧“build目录未跟踪”描述、噪声旧吸收任务和三输出模式等过时入口。Git实际列出构建四文档，未改.gitignore。

阅读全文核对CODE、COMMENT、CONCURRENCY、CACHE、COMPATIBILITY、OPTIMIZATION、NUMERIC，重写前六项；旧文保存在STANDARDS_SOURCE.txt。科学单位/浮点正文保留，其历史诊断附录仍待移出。标准统一接口/寿命/资源责任，区分本地查询与外部网络、受控多轴预算与隐藏嵌套、科学容差与逐位复现，新增E30。

累计71份原始文件全文重写（含根与目录入口），另有已读保留与归并主题；该数包括短导航，不能把导航改写量当作超长科学算法或实验精读的替代。科学问题仍25项，工程记录30项；全部剩余范围继续如实列出。

## 调度与异步队列续整理

目录/标准主题 `10e78d380` 已立即推送。全文阅读 PIPELINE_BLOCK、SCHEDULER、ASYNC_IO，核对完整scheduler.h与async_io.h、executor注册头、两类事件schema；重写后两份合同，旧文保存在SCHEDULING_SOURCE.txt。纠正探针与stdout协议同格式、Mosaic全局拟合可按窗口完全独立、错误取消后必排空、容量最低1即满足预算等过强描述。新增E31–E32，本轮未运行或修改调度/队列。

累计73份原始文件全文重写，25项待推导问题、32项工程记录。仍按清单处理超长算法、剩余工程/detail及实验正文。

## FITS/HiPS与AIO续整理

调度主题 `1bef89b3b` 已立即推送。全文阅读FITS_IO、HIPS_IO、AIO detail及common两份统一对象/模型页；重写前三份，区分生产物理值读取、低层存储值流式C接口、内部C++发布writer。核对fits.h全文、C接口头相关段、aio_fits.cpp实际缩放位置及发布机制，新增E33–E34。

原文完整保存在IO_SOURCE.txt；正式文档不再把归档目标、纯C接口、格式库枚举和阶段发布能力混为一体。累计76份原始文件全文重写，科学问题25项、工程记录34项；检查只覆盖文档结构，不是I/O运行验证。

## 配置、CLI与运行清单续整理

I/O主题 `759541170` 已立即推送。全文阅读CONFIG、CLI_PROTOCOL、CLI detail及运行清单合同，核对完整command_tree、cmd_verify、write_run_manifest与预检/parser相关分支。重写MANIFEST_VERIFY与CLI detail，修正协议中的取消/源码镜像描述，原文保存在CLI_RECORDS_SOURCE.txt。

新增E35–E36，区分doctor实际校验、未接线画像hash、收尾now时间与真实运行起止、flush/rename与持久化确认、运行JSONL与非运行单JSON。没有运行doctor、生成模板或修改CLI代码。累计78份原始文件全文重写，25项待推导与36项工程/文档记录。

## CPU后端、画像与loader续整理

CLI主题 `922ecfb65` 已立即推送。全文阅读benchmark两页、BACKEND、CAPABILITY_PROBE、AVX2_PROVIDER、ISA_VARIANTS和SECURE_LOADER，重写这7份正文，原文含全部历史读数/位表/测试矩阵保存在CPU_SOURCE.txt。实际核对当前CMake相关目标、AVX2查询门面、capability的classify、CLI画像生成/保存与消费及secure_loader平台分支。

正式解释区分两族ABI、清单、能力位、门面/计算TU、实际路由与可构建DSO；不将共享源当独立oracle，不从两次同阈值比较推出科学误差等价。新增Q26及E37–E39，保留原数值容差与算法；没有执行benchmark、反汇编、加载或科学运行。

累计85份原始文件全文重写，26项科学/数值待核验问题，39项工程记录。长篇算法、GUI/测试、部分detail/存储及实验资料继续处理。

### GUI 与浏览组件重写

全文核对 GUI_DESIGN、hips_browser、CLI 事件合同及 STF 入口，重写两页并逐字节保留 GUI_SOURCE.txt。区分完整 GUI 目标、已有 Qt 浏览组件、规划工程、实际事件字段与显示算法；登记 E40–E43，不修改显示/分组算法。清单累计重写原文件 87 篇，仍有未精读主题；此数字不代表全仓完成。本轮未构建 Qt 或执行事件回放。
