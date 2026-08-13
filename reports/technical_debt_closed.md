# 技术债清理（V14）

## 已关闭

1. **UPM component 语义**：`components=39489`（无意义）→
   `data_component_count=1` / `geometry_component_count=1` /
   `unobserved_geometry_nodes=39488`；无观测节点 sentinel component，
   不进入 reference-frame gauge；无 `frame_index[sentinel]` 隐式插入。
2. **config 默认值多源**：`config_consistency_check.py` 固化 struct/parser
   一致性（PASS）。
3. **浏览器 STF**：Auto Global 闪烁（pan/zoom 改标尺）→ robust 标尺
   保持；stretch 变化重新采样 → stretch-only redraw。
4. **文档缺失**：README 唯一入口 + 11 份 contract/dev/validation/性能文档。

## 待办（不阻断）

- 性能 profile（G7，用户指令最后做）。
- dense-field 正式合成用例并入 synthetic_gate（G4 已用独立工具验证）。
