本目录是构建与发布正本：`BUILD_GRAPH.md` 是生产构建图，`BUILD_NODES.md` 是构建节点
与工具链，`RELEASE.md` 是版本、交付产物与发布门槛。给做构建与发布的开发者阅读。

**本目录当前不在版本控制内，属仓库侧缺陷。** 根 `.gitignore` 有一条未锚定的 `build/`
规则，它匹配任意深度的同名目录，因而把本目录整层排除：

```bash
git check-ignore -v docs/engineering/build/BUILD_GRAPH.md   # .gitignore:20:build/  docs/engineering/build/BUILD_GRAPH.md
git -c core.quotepath=false ls-files docs/engineering/build | wc -l   # 0
sed -n '20p' .gitignore                                    # build/
```

后果：干净克隆上本目录不存在，文档索引门与文件树的双向比对恒绿（索引侧看不到、树侧也
看不到），`BUILD_GRAPH.md` 的复算链没有输入件。订正属仓库侧而非文档侧，需把该规则锚到
仓库根（`/build/`）并补 `!docs/engineering/build/` 放行，再把本目录四份正本入库。
本目录内的文档不因此改写内容，等入库后按 `BUILD_GRAPH.md` 的复算节重导一次机器块。