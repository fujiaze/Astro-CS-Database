# HISS 元数据规范

> 本页面为已冻结规范，源自 `02_FROZEN_STAGE1_HISS_SPEC.md`。Agent 可实现和文档化，不得自行改写科学语义。

## 1. 元数据原则

采用"传统 FITS 常用头 + HISS/HEALPix 必需参数"的精简方案，**不建立大型状态数据库**。

## 2. 必需空间/容器信息

- MAGIC / schema / header length / endian / feature flags；
- Tile 与子块目录；
- `PIXTYPE=HEALPIX`；
- `NSIDE`；
- `ORDERING=NESTED`；
- `RADESYS=ICRS`；
- `TILENSID`；
- `PIXFRAC`；
- signal/support 类型和语义；
- Gaia 相对测光系统与比例。

## 3. 传统 FITS 字段

按输入实际存在继承常用字段，如：

- `OBJECT`；
- `DATE-OBS`；
- `EXPTIME`；
- `FILTER`；
- `TELESCOP`；
- `INSTRUME`；
- `GAIN`；
- binning 等。

## 4. 校准字段

保留实际必要字段：

- `CALMODE`；
- `DARKREQ`；
- `DARKMODE`；
- `DARKSCL`；
- Master 标识和必要 HISTORY。

## 5. 不保存完整 WCS/SIP

HISS 像素已由 NSIDE、NESTED ipix、Tile 父像素和 ICRS 直接定位。原始 WCS/SIP 只用于 Stage1 内部从源图像映射到球面，**不写入 HISS**。

可选保留少量解算质量摘要，但不是定位必需字段。
