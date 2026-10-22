# 独立验证

当前仅声明编译和规范完整性检查，没有科学实现测试。未来按VERIFICATION生成全新analytic/enclosure/MC oracle，不import src或复用被测求解helper。输入/请求/容差先冻结，再运行被测程序；truth/observed/comparison/manifest分开，失败保留attempt。小矩阵API、contract进程和三命令分别验证，不复制旧测试。
