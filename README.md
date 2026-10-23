# ACSD v2 设计与重构

从[唯一架构入口](docs/engineering/rebuild/README.md)开始。分支rebuild/acsd-v2，设计基线9073a656b；远端截至04c8231d已加入122个后续提交，包含科学库、CLI/合同驱动、GUI及部分验证。实现存在不等于设计全部完成或生产验收通过；阶段记录见[现有实现交付](https://github.com/fujiaze/Astro-CS-Database/blob/04c8231d2f7888319a51fa4095265fc1285dc00d/validation/audit/issue-comment-112.md)。

本次仅补全文档，保留远端实现、声明、schema、构建和全部历史。资源子域、流式封存、开发探针、CPU/GPU执行等新增合同是下一轮实施目标，具体迁移次序/未接入状态见[API迁移清单](docs/engineering/rebuild/API.md#本次工程目标与声明迁移清单)。不能因文档出现目标方法或配置就认为现有程序已支持。

```text
python -m pip install -r validation/requirements.txt
cmake --preset design
cmake --build --preset design
ctest --preset design
python tools/check_design.py
```

上述为现有设计检查入口，不证明新增目标已实施；实际科学构建与验证按现有CMakePresets、VERIFICATION和阶段报告。首期仍为Windows/Linux amd64 CPU；GPU只冻结扩展边界，不承诺已提供或已测加速。

[工程补全及定向复核](docs/audit/engineering-completeness.md)记录本次范围、发现、处理和验证边界。[历史清理](docs/audit/CLEANUP.md)保留旧树追溯。不改main、不强推、不重置分支、不创建PR。
