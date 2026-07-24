# 14 发布与版本管理 Spec

## 1. 发布内容

- orchestrator.exe；
- 所需 DLL；
- Qt 浏览器及运行库（若该发行包含）；
- 默认配置和 schema；
- 数据格式说明；
- 版本/依赖 manifest；
- licenses；
- release notes；
- smoke test；
- SHA-256。

不发布源目录中的临时 DLL 或不明来源依赖。

## 2. 发布版本

- 应用：SemVer；
- HISS/HCSD：独立格式版本；
- DLL ABI：模块 ABI 版本；
- 配置：schema version。

版本信息必须能由 CLI 查询。

## 3. 发布候选流程

1. 冻结 commit；
2. 干净机 bootstrap/build；
3. 全部 Gate 适用项；
4. 生成 manifest 与 SBOM/依赖表；
5. 创建 release candidate；
6. 独立复核；
7. tag；
8. 发布；
9. 保存证据；
10. 验证回滚。

## 4. 兼容性

每个发布说明：

- 可读哪些 HISS/HCSD 版本；
- 可写哪个版本；
- 与旧 DLL/CLI 是否兼容；
- 配置迁移；
- 已知限制。
