# Wiki 更新要求

## 目标

在修改实现前，把仓库 Wiki 变成唯一有效契约来源，避免 Agent 继续读取旧 Python 原型或旧“v1/v2”文档。

## 必须新增或重写的页面

建议至少形成以下页面；可适配现有 Wiki 命名，但必须保持内容分离：

1. `Stage1-Scope-and-Pipeline.md`
   - CLI/GUI边界；
   - 单色输入；
   - 固定流水线；
   - 星点复用。

2. `Stage1-Calibration.md`
   - 三种校准公式；
   - Bias/Dark语义；
   - 最优 Dark 失败诊断和曝光比例回退；
   - Flat只做格式/结构校验。

3. `Stage1-Photometry-and-SNR.md`
   - Gaia积分校准在Drizzle前应用；
   - HISS相对测光累计通量；
   - 精简SNR控制点。

4. `Stage1-HEALPix-Drizzle.md`
   - 自动NSIDE；
   - NESTED；
   - pixfrac；
   - float64几何与通量守恒；
   - support。

5. `HISS-Container-and-Tiles.md`
   - Tile自适应规则；
   - FULL/BITMAP/SPARSE；
   - 独立子块；
   - XISF式Header+attachments；
   - 每块codec/transform/checksum字段；
   - 不保存WCS/SIP。

6. `HISS-Metadata.md`
   - 精简FITS风格字段；
   - 容器、HEALPix、测光、校准必需字段；
   - 字段用途注释。

7. `Stage1-Decision-Status.md`
   - 已冻结；
   - 待C++实验；
   - 明确不在本阶段。

8. `Stage1-Agent-Execution.md`
   - 单次连续执行；
   - 何时才允许停下；
   - 禁止Stage2和710回归；
   - 最终精简ZIP要求。

## 旧内容处理

- 与冻结规范冲突的页面必须重写、删除或明确加醒目标识：`SUPERSEDED — DO NOT IMPLEMENT`。
- 旧 HISS v1/v2、Python-only格式和静态示意图不能继续作为实现依据。
- 若保留历史页面，必须从正式入口 Wiki 断开，并说明其唯一用途是迁移参考。
- 删除“每个阶段都等待用户确认”的旧规则，替换为本包的连续执行规则。

## Wiki 自检

提交前搜索以下冲突词并逐一处理：

- `RING` 作为HISS内部格式；
- `surface brightness` 作为主signal；
- 未应用 Gaia 比例的 ADU signal；
- 固定4096或任意数组分块；
- HISS v1/v2双分支；
- Python writer作为正式实现；
- Footer/Checkpoint/断点续写；
- 保存完整WCS作为HISS空间定位；
- 最优Dark失败即终止且不回退；
- 自动执行Stage2或710回归。
