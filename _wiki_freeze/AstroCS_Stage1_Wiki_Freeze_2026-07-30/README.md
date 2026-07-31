# AstroCS Stage1 Wiki Freeze Pack

本包将前几轮对话中已经确认的 Stage1 决策整理为一套 GitHub Wiki 页面。

## 内容

- `wiki/`：可直接复制到 GitHub Wiki Git 仓库的 Markdown 页面；
- `tools/Push-Wiki.ps1`：带 Git 命令超时的受控同步脚本；
- `MANIFEST.json`：文件哈希。

## 使用

首次需要先在 GitHub 网页上创建一个 Wiki Home 页面，然后执行：

```powershell
pwsh -File .\tools\Push-Wiki.ps1 `
  -Repository "OWNER/REPOSITORY"
```

以上只会在本地克隆的 Wiki 中生成 commit，不会 push。

检查无误后：

```powershell
pwsh -File .\tools\Push-Wiki.ps1 `
  -Repository "OWNER/REPOSITORY" `
  -Push
```

## 当前冻结范围

- Stage1 only；
- 单色数据；
- GUI负责分组、匹配、警报和交互；
- CLI只接受已有 Master；
- 两种校准公式；
- float32 HISS signal/support；
- 稀疏 float32 SNR 控制点；
- 自动 NSIDE 1–2×过采样；
- 高精度 Drizzle；
- 功能完成后先提交性能分析；
- 禁止 Agent 自行启动710帧。
