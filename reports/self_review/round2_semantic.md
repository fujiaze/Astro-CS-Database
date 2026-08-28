# Round 2 — Semantic（V18R2）

## 检查项

1. **gaia 剪枝只改"必然相离"拒绝**：极投影平面圆盘 vs 轴对齐矩形判定，
   拒绝仅发生在数学上圆与矩形无交时（点-矩形最近距离 > 半径×1.2）；
   未改变叶子内逐星过滤（mag/角距条件原样）。
2. **候选集合不变**：candidate oracle 9003/9003；899 星逐颗 ra/dec/mag
   一致（旧/新 DLL stash 对照）。
3. **overlap 判定边界**：quick-reject 安全余量方案——拒绝集 ⊂ 原拒绝集；
   边界带走原始 acos（位级一致）。
4. **Drizzle scratch 复用**：thread-local 复用对象（candidates/drop
   corners/DropGeometry），构建语义与逐像素分配一致；clip_normals
   清空 bug 修复后几何正确。
5. **行级顶点缓存**：同一 WCS 角点转换，逐位同值（仅缓存位置不同）。
6. **SHA 归一化**：相同算法（FIPS 180-4），配置 SHA 验证一致。
7. **data_pipeline 删除**：astro_image_io 为 canonical（superset），
   无生产引用。

```text
ROUND2=PASS（无语义冒名/双路径）
```
