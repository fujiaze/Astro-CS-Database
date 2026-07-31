# 测试、验收与性能报告

## 1. 科学正确性

至少覆盖：

- 三种校准公式；
- Bias只减一次；
- 最优Dark成功路径；
- 最优Dark失败后诊断并回退曝光比例；
- 回退也不可用时硬失败；
- Gaia比例在Drizzle前应用；
- 单源像素通量守恒；
- 多像素球面重叠通量守恒；
- pixfrac=1、典型小于1和接近0边界；
- support处于0～1；
- 明显support超限触发错误；
- 自动NSIDE覆盖局部最细WCS/SIP尺度；
- NESTED ipix和Tile父子恢复正确。

## 2. HISS格式

至少覆盖：

- FULL、BITMAP、SPARSE_LIST往返；
- signal/support/SNR独立读取；
- RAW子块；
- 多codec声明机制；
- 未知可选子块可跳过；
- 未知必需子块拒绝；
- offset/size越界拒绝；
- checksum错误定位到具体子块；
- `.partial`不会被普通Reader当正式HISS；
- 原子提交后正式文件可读；
- Header不含完整WCS仍可正确天球定位。

## 3. CLI

- 参数和错误码稳定；
- 用户显式合法NSIDE不被修改；
- Flat仅做允许格式和结构校验；
- 回退事件清晰输出且写入HISS元数据；
- 不自动启动Stage2或710回归。

## 4. Browser

- 首次打开不加载全部signal；
- 能按区域定位Tile；
- signal/support/SNR切换正确；
- 无数据、边缘、孔洞和Tile边界可检查；
- 数值查询与C++ Reader一致。

## 5. 性能剖析

提交前对代表性Stage1运行做详细C++ profile：

- 校准；
- PlateSolve/星点检测；
- PSF；
- Gaia积分测光；
- NSIDE计算；
- WCS/SIP与球面Drizzle；
- Tile聚合；
- occupancy编码；
- 子块压缩；
- Header组装和写盘；
- HISS打开、首Tile、随机Tile、连续Tile；
- Browser打开和首次可见数据；
- 总wall/CPU/峰值RSS/I/O。

性能报告只说明瓶颈和建议，不得降低NSIDE、简化WCS、改用近邻、跳过小贡献或牺牲精度。

## 6. 测试规模

- 允许使用小型合成数据和少量代表性真实帧。
- 禁止未经用户授权运行710帧全量回归。
- 不运行Stage2。

## 7. 验收状态

Agent可报告：

- 测试通过；
- 已知失败；
- 未运行原因；
- 性能结果。

Agent不得自行宣称“用户验收完成”。
