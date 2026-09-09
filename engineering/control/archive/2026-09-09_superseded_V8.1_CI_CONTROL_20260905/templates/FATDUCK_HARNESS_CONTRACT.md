# Fatduck 本地 Harness 合同

固定入口：`D:\AstroCSRunner\harness\run-validation.cmd`。

允许动作只有：校验 candidate → benchmark → Phase1/2/3 分段运行 → 资源采样 → 生成本地完整结果 → 生成公开白名单结果。禁止 git、编译、安装软件、上传 testdata、调用任意仓库脚本。

必需参数：`--source-sha`、`--candidate`、`--publish`。实际数据根目录来自本机只读配置，不从 workflow 传入或打印。

必需本地文件：

- `harness.lock.json`：harness 版本和所有脚本/exe SHA256；
- `data_manifest.private.json`：32R 和 masters 的逻辑 ID/hash，永不上传；
- `publish_policy.json`：与仓库版本 hash 相同；
- `runner_config.json`：并发、路径、保留期，不含凭据。

退出码：0=全部机器门禁通过；10=candidate/provenance 错；20=数据 manifest 错；30=科学/功能错；40=资源/性能错；50=公开白名单错；60=环境错。任何失败仍生成脱敏 summary，不生成 PASS。

公开输出必须先写临时目录，经扩展名、magic byte、大小、EXIF、敏感关键词和路径扫描后原子移动到 publish。完整结果永不从 runs 复制到 publish。
