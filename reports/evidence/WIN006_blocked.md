# WIN-006 阻断记录(BLOCKED) — 银心真实数据缺失

## 阻断对象
WIN-006 需要"从本机数据选三板块少量 R 帧+masters; hash manifest; 跑代表链路并自动监控"。本机(测试包约定数据源 `testdata/`)没有目标数据集 **Galaxy_Center_T4(银心, T4, 3 panel mosaic)** 的实际原始帧与 T4 母版。

## 实测命令与结果(2026-08-30)
1. `Get-ChildItem Astro-CS-Database/testdata -Recurse -File` → **仅 `index.json`**(Count=1)。所有 "框架/母版" 均未入库。
2. 全盘 `Get-ChildItem C:/Users/fujia -Recurse -Include *.fit,*.fits,*.fts,*.xisf` → 仅 M106(Desktop) 等其它目标 + numpy 测试数据; 无 Galaxy_Center 帧、无 T4 母版。
3. 搜 `Galaxy_Center|T4 calib|panel|lights` 目录 → 仅 `工程控制/evidence/P11-003|P12-004`(旧任务报告/raw_logs, 非原始帧)。
4. 搜 D:/E:/F: 盘 FITS → E: 有 NGC869 Light、F: 有 LSST 合成用例; **无银心 frames/T4 母版**。
5. `if(Test-Path run/){...}` → **无 run/** 目录(无缓存校准/产物)。
6. 数据获取路径: 控制包/仓库**无自动下载/预备脚本**; index 中 Galaxy_Center_T4 **无 external_source URL**(对比 NGC247 有 Baidu Pan 链接, 仍需登录凭据, 非自动可达)。

## 影响范围
- 依赖链 WIN-006 → WIN-007(银心 32R) → WIN-008(HiPS) → REV-003(32R 摘要/截图/资源摘要) 全部依赖同一份银心真实数据 → 同为数据缺失。
- WIN-009(Windows 发布包/SBOM/hash/smoke/模拟无 profile): **数据无关**, 可用现有 exe 独立验证。

## 需求
需人工提供/下载 `testdata/Galaxy_Center_T4/lights`(R 帧: panel1=11, panel2=11, panel3=10)与 `testdata/T4 calibration files`(T4 母版: bias/dark/flat)到 Fatduck 后, WIN-006/007/008 方可执行。数据缺失期间, 继续可执行的 WIN-009 打包验证。
