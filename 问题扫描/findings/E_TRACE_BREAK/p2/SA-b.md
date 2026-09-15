# SA 片4-5 · P2 两条

### SA-N-04（P2）门禁与接口裁决的权威依据文本在 HEAD 不可解析：控制包标准 8 种 / 21 出现
- 站点：12_DLL_ABI_AND_LOADER_STANDARD.md（7）、11_MODULE_SOURCE_TEST_STANDARD.md（4）、15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md（3）等。find 仅命中 工程控制/AstroCS_V7…FINAL3/ 副本、git ls-files 零命中 ⇒ 据以判合规的规范文本对 clean checkout 不存在。处置：入库 engineering/control/archive/ 或改述为 40 文件裁决条款号。related A-31、M6b-E-002 族、簇 7、SA-N-03

### SA-N-05（P2）硬底线：硬悬空 48 种 / 55 出现 + 产物名 34 种 / 75 出现 + 外链未外部化 7 种 / 18 出现
- 硬悬空代表：tests/abi/run_abi_checks.sh（4，HEAD 与全历史双零命中）、ACCEPTANCE_GATES.md（4，被当冻结文档引用而全仓不存在）、SNR_REDESIGN_CONTRACT.md（3）；PHASE_OVERVIEW.md（2，宪章 §12.1 点名而全仓不存在）该事实已在册 M5b-G-07，此处只登记锚面处置：指历史者改述曾存在、指现行者整句撤、禁改锚到同名近似宿主。
- 产物名类（product.json 9、p1_wcs.json 6、stage1.json 5、oracle.jsonl 5 及建议新增夹具名）非宿主文件 ⇒ 标运行期生成产物并链到生成或消费侧代码宿主。外链类（star_finder.c 7、healpix.c 6、atpmatch.c、gsl 系统头、siril raw URL）宣称对象正确而形态错 ⇒ 须显式外部化（外部加仓库加版本加路径），否则任何锚门永判漂。
- 批量回写四步安全序列（依 SA_anchor_repair.json，1008 种全数判定零遗留）：① 只跑 auto_safe 687 种 / 2382 出现，替换带路径字符边界且先长后短（anchor_repair v1 二次污染教训）；② needs_human 122 种 / 240 出现按行内 candidates 人工签；③ reword 类逐句处置并回写账本 fix_note；④ 复跑 verify_anchors.js 断言悬空仅剩白名单。QA：2380 个建议锚经宿主存在与符号子串双校验零失败。related C-18、E4、E13、M8a-G-001、A-44
