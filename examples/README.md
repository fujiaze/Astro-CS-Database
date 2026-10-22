# 最小合同示例

[to-fits.request.json](to-fits.request.json)是完整无占位ID输入。实现后调用：

```text
acsd-contract --request examples/to-fits.request.json --response result.json
```

输入PixelCenter0(0.25,2.5)，返回FitsPixel1(1.25,3.5)，见[期望模型](to-fits.expected-model.json)。模型data对应result.value；运行时的hash/build/usage由真实driver生成，不伪填。此分支无driver，本例为合同夹具，尚未实跑。

[WCS示例](wcs.request.json)完整调用make_wcs，返回crpix=[1.25,2.5]、degree转rad的镜像CD与完整单射/域证书。将结果Wcs typed对象及依赖完整放入input_objects再提供给forward，point=PixelCenter0(.25,1.5)须返回[1,0,0]方向及数值误差。输出不能预填零归一误差。无旧产品依赖。

配置遵循schemas/config.schema.json，scientific引用typed请求文件。科学tolerance、响应、历元和尺度搜索域必须填，无全仪器零配置。

U01完整矩阵调用：acsd-contract --request examples/u01.request.json --response result.json。输入24列整数F精确给FFᵀ=C，经R与signed t得到方差真值4.328125，见u01.expected-model.json；实际返回数值包围只需包含真值，不要求宽度为0。该夹具及schema已验证，科学驱动尚未实现。

external-array.request.json演示无需product的大数组块入口（示例块为24byte）：相同R作用真实[2,4,8]块得到[3,7]。descriptor/hash、别名闭包及shape均由design检查核对。超过65536元素使用同一入口，不使用不同算法或临时schema。
