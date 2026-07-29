# TestData T1–T4 设备与数据清单规范

Agent 必须递归读取每个 TestData 子文件夹的说明文档，并交叉读取 FITS/XISF Header，不得仅凭目录名猜测。

每套设备档案至少包含：设备 ID、望远镜、口径、焦距、相机、像元、图像尺寸、Bin、Gain、Offset、温度、滤镜集合、Light 目录、Master 目录、文档来源和冲突说明。

输出：

- `TESTDATA_EQUIPMENT_CATALOG.csv`
- `TESTDATA_DATASET_CATALOG.csv`
- `FILTER_ALIAS_MAP.json`
- `DOCUMENT_FACT_CONFLICTS.md`

硬门限：只允许 T1–T4 四套规范设备 ID；所有 Light 必须能归属其中之一，未知项必须作为错误处理。
