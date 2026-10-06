# ACSD 细节文档索引

本索引覆盖细节文档全树。细节文档由一级正本推理产出，只做展开，不另立口径；冲突时以更高一层为准。

```text
docs/detail/
├── 00_INDEX.md
├── README.md
├── common/                 统一模型、统一对象、跨阶段公共机制
│   ├── unified_model.md
│   ├── unified_objects.md
│   └── shared_libraries.md
├── normalize/              单帧标准化管线与模块
│   ├── pipeline.md
│   └── modules/star_detection.md
├── mosaic/                 马赛克合成管线与模块
│   └── pipeline.md
├── export/                 投影导出管线与模块
│   └── pipeline.md
├── infrastructure/         运行时、命令行、可观测性、日志错误、排障、产品存储形态
│   ├── runtime.md
│   ├── cli.md
│   ├── aio.md
│   ├── benchmark.md
│   ├── observability.md
│   ├── log_and_error_system.md
│   ├── troubleshooting.md
│   ├── storage_form.md
│   ├── gaia_xpsd_client.md
│   └── hips_browser.md
└── registry/               模块登记正本（25 张模块卡）
    ├── acsd.phase1.session.md
    ├── acsd.phase1.calibration.md
    ├── acsd.phase1.cosmetic.md
    ├── acsd.phase1.star-detection.md
    ├── acsd.phase1.star-psf.md
    ├── acsd.phase1.wcs-platesolve.md
    ├── acsd.phase1.photometry.md
    ├── acsd.phase1.noise-snr.md
    ├── acsd.phase1.drizzle.md
    ├── acsd.phase1.hips-writer.md
    ├── acsd.phase1.writer.md
    ├── acsd.phase2.session.md
    ├── acsd.phase2.coverage.md
    ├── acsd.phase2.sample.md
    ├── acsd.phase2.upm-fit.md
    ├── acsd.phase2.upm-apply.md
    ├── acsd.phase2.reject.md
    ├── acsd.phase2.integrate.md
    ├── acsd.phase2.write.md
    ├── acsd.phase2.resample.md
    ├── acsd.phase3.properties.md
    ├── acsd.phase3.wcs.md
    ├── acsd.phase3.resample2.md
    ├── acsd.phase3.writer.md
    └── acsd.phase3.verify.md
```

## 各目录内容

统一模型与跨阶段公共机制在统一模型页与共享基础库页交代，统一对象登记在统一对象页，三者共同构成全部管线的公共语言。单帧标准化管线按节点顺序描述从原始帧到球面单帧产品的完整链路，星表引导检测的逐算子规格在星检测模块页展开。马赛克管线描述从球面单帧集合到马赛克产品的相对定标、排异与集成链路。导出管线描述从球面产品到平面科学文件的投影与写出链路。运行时与支撑面的落地约束在基础设施各页交代：调度执行、命令行入口、输入输出与原子提交、机器画像、可观测性、日志错误系统、排障手册、产品存储形态、星表客户端、球面浏览器。每个生产模块的端口、配置、并行、内存、错误与验证口径在模块登记页逐模块登记。

## 登记面与落地面的分工

模块的接口签名、公共头、核心符号与块生命周期由模块登记页承载：每一张登记卡写清该模块读哪些命名块、写哪些命名块、块的元数据与交接方式，以及块被全部消费者用完后由管线显式销毁的归属。基础设施的管线页承载阶段流程：各阶段按什么顺序经过哪些节点、每个节点调用哪个模块、运行帧如何在块间流水。读阶段流程先看管线页，查某个模块的块读写与销毁责任看登记页，两处口径一致。

## 管线页与模块页的落位

三条管线页分别落在单帧标准化、马赛克合成与投影导出三个目录：单帧标准化管线页讲从原始帧到球面单帧产品的节点顺序，星检测模块页讲星表引导检测的逐算子规格；马赛克管线页讲相对定标、排异与集成的固定链路；导出管线页讲从球面产品到平面科学文件的投影与写出链路。各管线经过的模块在模块登记目录按模块一页登记，管线页只写节点顺序与模块调用关系，不重复登记卡的接口与生命周期。

## 阅读顺序

先读统一模型页建立数据对象与观测模型的公共语言，再按所涉命令读对应管线页，然后读相关模块登记页，最后按需查基础设施页。

## 与一级正本的关系

一级正本是科学分册与工程分册，细节文档引用一级已定稿的公式与结论，不重复推导。数值表与推导留在科学分册，字段与行为定义留在工程合同，细节文档只记录模块如何把它们落地为读写块、接口签名、步骤分支、配置来源、相邻关系与调试入口。

## 参考文献

[1] Horne K. An optimal extraction algorithm for CCD spectroscopy. Publications of the Astronomical Society of the Pacific, 1986, 98: 609–617. https://doi.org/10.1086/131801
[2] Bertin E., Arnouts S. SExtractor: Software for source extraction. Astronomy and Astrophysics Supplement Series, 1996, 117(2): 393–404. https://doi.org/10.1051/aas:1996164
[3] Fernique P., Allen M. G., Boch T., Burke D., Castro-Ginard A., Davidson J., Durand D., Kreckel K. Hierarchical progressive surveys: Visualisation and streaming of astronomical images and catalogues with HiPS. Astronomy and Astrophysics, 2015, 578: A114. https://doi.org/10.1051/0004-6361/201526075
