# Fatduck 与发布任务

## V8-FAT-001｜一次性安装

- 创建专用本地账户和 `D:\AstroCSRunner` ACL。
- 安装 repository-level runner，仅 label `fatduck-realdata`。
- 设置本地 testdata 只读、runs/publish 可写。
- 安装固定 harness，生成 `harness.lock.json`。
- 验证普通/PR workflow 无法匹配 runner；验证 runner 无 Git push 和个人目录权限。

## V8-FAT-002｜自动真实数据运行

- 只处理 selector 给出的最新合格 source SHA。
- 校验 candidate/harness/data manifest。
- benchmark 后分段执行 Phase1/2/3；32R 正式运行只做一次。
- 全程监控；CPU-heavy 低利用率、内存增长、假进度、接缝门禁失败均返回 FAIL。
- 完整结果留本机；生成公开目录和 Owner 打开命令。

## V8-FAT-003｜公开边界与提醒

独立只读 Agent 对 publish 目录做扩展名、magic bytes、大小、EXIF、绝对路径、FITS header 关键词和原始文件名扫描。通过后才上传。GitHub-hosted notify job 更新同一个 Owner Review Issue，包含 JPG、指标、source SHA 和本地打开命令。

## V8-AUD-001/V8-AUD-002｜抽查而非自报

独立 reviewer 抽查：三条 SCI→TEST 链、两项 ABI、两个 heavy trace、一个故意失败 CI、三个模块 README、Phase2 操作图、provider 真实入口、Fatduck 白名单。审核者不修改其发现的问题。

P0/P1 自动派发修复并复审；只有最后 `READY_FOR_OWNER_DECISION` 需要 Owner 查看 JPG/本地结果并决定发布 alpha。
