# Schema边界

JSON Schema校验envelope、CLI输入形状、资源和事件；科学typed对象逐字段解码规则由contracts.hpp与API.md唯一拥有。schema不会把任意object当科学有效输入；未知字段/未定义type/缺必需字段必须由typed decoder拒绝。operation白名单由公开声明固定，不允许任意helper。

science-input.schema.json和cli-science.schema.json从contracts.hpp自动导出实际字段/类型/每operation参数；contract.schema直接引用。更新声明后运行tools/generate_contract_schema.py，check_design检查无漂移。产品持久化读者按OBJECTS的规范hash/块闭包另作语义验证，不能用input schema代替。
