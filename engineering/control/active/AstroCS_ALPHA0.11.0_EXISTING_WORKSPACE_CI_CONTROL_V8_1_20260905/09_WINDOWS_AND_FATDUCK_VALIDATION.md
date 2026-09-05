# 09｜GitHub Windows 与 Fatduck 验证

## GitHub Windows 编译合同

- Runner image：`windows-2022`，amd64。
- Visual Studio：2022 major 17，MSVC v143；记录 runner image、cl、link、VCTools、SDK、CMake 的实际完整版本。
- Windows SDK 最低目标为 Windows 10；源码不得无合同使用 Windows 11/Server 独占 API。
- CMake 配置：Release、tests ON、ACR OFF、CPU providers ON。
- 输出：CLI exe、各模块 DLL、运行时依赖、许可证、版本、`BUILD_PROVENANCE.json`、`SHA256SUMS`。
- GitHub CI 运行 DLL 加载、ABI、CLI、合成科学和安装布局测试。它不得访问 Fatduck 数据。

托管镜像会更新，不能把未必存在的 MSVC patch 号写死；冻结的是 VS 2022/v143/Win10 最低目标和可追溯工具链清单。若编译器变化引起结果变化，由合成 Oracle 判定，不能直接放宽容差。

## Fatduck runner 合同

Fatduck 仅接收 `main` 上 Linux/Windows CI 均成功的 candidate。它不得 checkout、编译、修改或 push 源码。

执行顺序：

1. 校验 candidate 完整 source SHA、artifact digest、所有文件 SHA256。
2. 校验本地 harness SHA 与 `harness.lock.json`。
3. 在空 candidate/run 目录解包，不覆盖上一成功结果。
4. 运行 `astrocs-cli benchmark`，生成绑定 CPU/OS/二进制 hash 的 profile。
5. 分别独立执行 Phase1、Phase2、Phase3 的合成/小真实样本 smoke。
6. 使用冻结 manifest 的银心 32R 做一次正式 Phase1/2/3 分段验证；不得重复跑历史版本。
7. 每个 heavy 区间记录真实 CPU/线程/RSS/PSS/IO/墙钟/进度，低利用率、单线程重计算或泄漏趋势 FAIL。
8. 生成完整本地 FITS/HiPS、接缝诊断、support/weight/rejection 产品。
9. 生成公开目录，运行白名单和脱敏检查，再上传。

## 接缝专项

真实数据验收至少输出：

- 重叠区中位背景差和稳健尺度；
- 接缝两侧法向梯度跳变；
- weight/support 连续性；
- photometric plane/UPM 应用前后对比；
- rejection 计数和异常边界；
- 固定 WCS、FOV、STF 的马赛克 JPG；
- 接缝增强 JPG 与 support JPG。

不得只凭肉眼 PASS。量化门禁来自当前 SCI/ALG 合同；如果合同没有阈值，先用合成实验推导并由 science owner 原子提交，不临场猜数。

## 允许上传

仅 `ci/publish_policy.json` 中的：JPG、结果/资源 JSON、CSV、JUnit、脱敏短日志。禁止上传原始 FITS、FITS headers、HiPS tiles、星表、绝对路径、机器用户名、原始数据文件名。

完整结果保留在：`D:\AstroCSRunner\runs\<sha>\`。公开 summary 给出本地命令：

```text
astrocs-cli review open --run <run-id>
```

GitHub-hosted notify job 更新单一 Owner Review Issue；Fatduck token 不具备 issue/repo 写权限。

## 离线

Fatduck 离线状态为 `FATDUCK_PENDING`，不阻塞任何 GitHub CI 或开发。定时 workflow 每 6 小时刷新最新候选；只保留一个 pending。上线后自动运行，无需 Owner 手动重派。
