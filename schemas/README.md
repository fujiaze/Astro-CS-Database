# Schema边界

JSON Schema校验envelope、CLI输入形状、资源和事件；科学typed对象逐字段解码规则由contracts.hpp与API.md唯一拥有。schema不会把任意object当科学有效输入；未知字段/未定义type/缺必需字段必须由typed decoder拒绝。operation白名单由公开声明固定，不允许任意helper。
