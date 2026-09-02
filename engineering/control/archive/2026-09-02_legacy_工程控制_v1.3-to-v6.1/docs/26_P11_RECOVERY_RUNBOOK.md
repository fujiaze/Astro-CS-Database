# P11 恢复执行手册

1. 导入 P11-004 review bundle，核对 SHA 和已有报告；
2. 将旧 P11-004 标记从 DEFERRED 恢复为 IN_PROGRESS，不删除已有证据；
3. 添加权威 inlier 导出接口或诊断回调；
4. 升级 `wcs_closure_diagnostic.py`：新增 `--authoritative-pairs`，该模式禁止 kd-tree 重新配对；
5. 在原 16 帧上运行 Gate v2；
6. 若全部通过：生成 `NO_CODE_CHANGE_REQUIRED.md` 和 ADR，P11-004 PASS；
7. 若失败：先证明误差类型，再修改唯一 WCS 生产端，重跑16帧；
8. 执行 P11-005：710帧 PlateSolve成功率/RMS回归，并对所有成功帧或分层抽样执行权威闭环；
9. P11-006 更新坐标契约、CLI capabilities、provenance；
10. 进入 P12 测光诊断。若 WCS B层通过、blind层失败，则问题归属 Photometric匹配而不是 WCS。

## 禁止

- 继续拿全星表 kd-tree p68 阻塞 P11；
- 删除 T2 帧；
- 只看可视化截图；
- 只使用内部 RMS；
- 未经权威星对失败就改坐标符号。
