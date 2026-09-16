# 任务：SCI-FIX-PROJ 投影/WCS 科学订正（R-1 结论落地）

状态：NOT_STARTED
层：L1　依赖：ARCH-001（锚点已同步，本次订正后需再同步一次）
文件域互斥组：S7-A（科学文档订正 + 对应实现/门）

#
#
 
依
据
（
负
责
人
指
令
 
+
 
研
究
线
证
据
）




负
责
人
：
「
*
*
我
不
要
遵
循
旧
制
，
我
要
求
科
学
文
档
必
须
是
正
确
的
。
*
*
」
⇒
 
科
学
文
档
错
就
改
（
见
 
`
工
程
控
制
/
P
R
O
J
E
C
T
-
G
O
V
E
R
N
A
N
C
E
-
0
1
/
S
C
I
E
N
C
E
_
C
O
R
R
E
C
T
N
E
S
S
.
m
d
`
 
与
 
`
E
N
G
I
N
E
E
R
I
N
G
_
S
P
E
C
.
m
d
 
§
3
`
）
。


证
据
：
`
r
e
p
o
r
t
s
/
P
R
O
J
E
C
T
-
G
O
V
E
R
N
A
N
C
E
-
0
1
/
r
e
s
e
a
r
c
h
/
R
-
1
_
投
影
W
C
S
数
学
正
确
性
.
m
d
`
（
4
9
1
 
行
）
+
 
`
r
u
n
/
P
R
O
J
E
C
T
-
G
O
V
E
R
N
A
N
C
E
-
0
1
/
R
-
1
/
*
*
`
（
a
s
t
r
o
p
y
 
7
.
0
.
1
 
逐
点
对
拍
、
候
选
补
丁
、
探
针
）
。




#
#
 
要
订
正
的
（
逐
条
；
每
条
都
要
给
证
据
引
用
）




|
 
#
 
|
 
订
正
内
容
 
|
 
落
点
 
|


|
-
-
-
|
-
-
-
|
-
-
-
|


|
 
1
 
|
 
*
*
C
A
R
/
A
I
T
 
必
须
把
 
`
C
R
V
A
L
2
`
（
含
 
L
O
N
P
O
L
E
 
默
认
规
则
）
纳
入
映
射
*
*
（
按
 
F
I
T
S
 
W
C
S
 
P
a
p
e
r
 
I
I
 
§
2
.
2
 
三
 
E
u
l
e
r
 
角
）
 
|
 
`
d
o
c
s
/
s
c
i
e
n
c
e
/
P
H
A
S
E
3
_
H
I
P
S
_
T
O
_
F
I
T
S
*
.
m
d
`
、
`
d
o
c
s
/
a
l
g
o
r
i
t
h
m
s
/
P
H
A
S
E
3
_
P
R
O
J
_
I
M
P
L
*
.
m
d
`
、
`
d
o
c
s
/
c
o
n
t
r
a
c
t
s
/
D
A
T
A
_
S
E
M
A
N
T
I
C
S
.
m
d
`
 
|


|
 
2
 
|
 
*
*
A
I
T
 
域
界
 
`
A
<
2
`
 
→
 
`
A
≤
1
`
*
*
 
|
 
同
上
（
A
L
G
 
约
 
:
4
2
4
 
的
「
=
2
」
是
错
值
）
 
|


|
 
3
 
|
 
*
*
C
A
R
 
n
a
t
i
v
e
 
极
行
（
θ
=
±
9
0
°
）
f
a
i
l
-
c
l
o
s
e
d
 
拒
绝
*
*
（
现
守
卫
 
`
>
9
0
`
 
恰
好
放
行
 
`
=
9
0
`
）
 
|
 
同
上
 
|


|
 
4
 
|
 
*
*
删
除
对
已
废
止
宪
章
的
引
用
*
*
；
投
影
集
合
以
 
`
A
S
T
R
O
C
S
_
D
E
S
I
G
N
 
§
5
.
3
`
 
的
*
*
八
投
影
*
*
为
准
（
A
L
G
 
§
1
5
 
现
写
四
投
影
，
其
唯
一
依
据
「
宪
章
 
§
1
8
.
1
」
在
活
动
树
中
*
*
不
存
在
*
*
）
 
|
 
A
L
G
 
§
1
5
 
全
节
 
|


|
 
5
 
|
 
*
*
r
e
g
i
s
t
r
y
 
升
 
v
3
、
v
1
 
退
场
*
*
；
`
c
o
n
t
r
a
c
t
s
/
s
c
h
e
m
a
s
/
p
r
o
j
e
c
t
i
o
n
_
r
e
g
i
s
t
r
y
.
s
c
h
e
m
a
.
j
s
o
n
`
 
的
悬
空
引
用
修
好
 
|
 
`
l
i
b
/
a
l
g
o
r
i
t
h
m
s
/
p
r
o
j
e
c
t
i
o
n
/
*
*
`
 
+
 
`
d
o
c
s
/
p
l
u
g
i
n
s
/
a
l
g
o
r
i
t
h
m
s
_
p
h
a
s
e
3
/
1
4
_
p
r
o
j
e
c
t
i
o
n
.
m
d
:
1
8
`
 
|


|
 
6
 
|
 
实
现
按
 
R
-
1
 
的
候
选
补
丁
修
正
（
`
r
u
n
/
P
R
O
J
E
C
T
-
G
O
V
E
R
N
A
N
C
E
-
0
1
/
R
-
1
/
p
a
t
c
h
/
p
3
_
p
r
o
j
_
v
6
_
f
i
x
e
d
.
c
p
p
`
，
已
与
 
a
s
t
r
o
p
y
 
对
拍
 
m
a
x
 
6
.
8
5
4
e
-
1
3
°
）
 
|
 
`
l
i
b
/
a
l
g
o
r
i
t
h
m
s
/
p
r
o
j
e
c
t
i
o
n
/
p
3
_
p
r
o
j
_
v
6
.
c
p
p
`
 
|


|
 
7
 
|
 
*
*
门
与
测
试
*
*
：
`
p
3
_
p
r
o
j
_
w
c
s
_
o
r
a
c
l
e
.
p
y
`
 
补
 
`
d
e
c
0
≠
0
`
 
用
例
 
+
 
`
C
R
P
I
X
↔
C
R
V
A
L
`
 
不
变
量
断
言
；
修
掉
只
在
 
`
|
C
R
V
A
L
2
|
≈
0
`
 
成
立
的
 
`
c
a
r
_
a
n
a
l
y
t
i
c
_
l
a
t
i
t
u
d
e
_
b
a
n
d
`
；
`
p
3
_
p
r
o
j
_
l
e
g
a
c
y
_
d
e
v
i
a
t
i
o
n
.
p
y
`
 
的
「
预
期
偏
差
」
改
 
x
f
a
i
l
/
显
式
表
 
|
 
`
t
e
s
t
s
/
*
*
`
、
共
址
测
试
 
|




#
#
 
参
考
条
文
（
P
3
-
0
0
2
 
的
 
8
 
条
一
并
落
地
）




`
M
1
a
-
A
-
0
0
7
`
（
实
现
对
、
文
档
错
：
C
D
⁻
¹
 
方
向
与
 
|
D
e
c
|
≥
5
°
 
守
卫
）
、
`
M
1
a
-
A
-
0
0
8
`
（
取
 
C
=
1
 
口
径
）
、
`
M
1
a
-
A
-
0
0
9
`
（
零
权
重
 
→
 
N
a
N
 
传
播
）
、
`
M
7
-
A
-
1
1
4
`
（
补
协
方
差
项
；
ρ
=
0
.
1
9
 
⇒
 
方
差
低
估
 
3
6
.
3
%
）
、
`
M
7
-
A
-
2
0
9
`
（
`
s
_
o
u
t
_
r
a
d
`
 
单
位
链
）
、
`
M
7
-
A
-
1
3
0
/
1
2
9
②
`
（
写
「
±
1
 
U
L
P
」
而
非
「
精
确
」
）
；
*
*
`
M
7
-
A
-
1
1
7
`
 
的
账
本
改
法
本
身
是
错
的
*
*
（
正
确
是
 
`
o
r
d
e
r
_
s
e
l
 
+
 
l
o
g
2
(
W
)
`
）
，
*
*
`
M
7
-
A
-
1
2
9
①
`
 
是
误
报
*
*
—
—
两
条
按
研
究
结
论
订
正
账
本
备
注
，
不
按
账
本
处
方
改
码
。




#
#
 
硬
纪
律




1
.
 
零
 
g
i
t
 
写
（
前
台
提
交
）
；
不
得
改
 
`
c
i
/
*
*
`
、
`
.
g
i
t
h
u
b
/
*
*
`
、
`
d
o
c
s
/
s
c
i
e
n
c
e
`
 
之
外
的
无
关
文
档
；


2
.
 
*
*
订
正
后
必
须
*
*
复
跑
锚
点
检
查
 
`
p
y
t
h
o
n
3
 
d
o
c
s
/
a
l
g
o
r
i
t
h
m
s
/
a
n
c
h
o
r
s
/
c
h
e
c
k
_
d
o
c
_
l
i
n
e
_
a
n
c
h
o
r
s
.
p
y
 
-
-
r
o
o
t
 
.
`
 
→
 
r
c
=
0
（
本
次
要
改
被
锚
文
件
的
行
号
，
必
须
同
步
锚
与
 
`
a
n
c
h
o
r
_
c
o
n
t
r
a
c
t
.
j
s
o
n
`
）
；


3
.
 
构
建
 
+
 
测
试
：
`
n
i
n
j
a
 
-
C
 
b
u
i
l
d
 
-
k
 
0
`
 
0
 
F
A
I
L
E
D
；
`
c
t
e
s
t
 
-
R
 
'
p
r
o
j
|
p
3
_
p
r
o
j
'
`
 
全
绿
（
含
新
增
 
`
d
e
c
0
≠
0
`
 
用
例
：
改
前
必
红
 
→
 
改
后
必
绿
，
给
两
次
输
出
）
；


4
.
 
在
 
`
S
C
I
E
N
C
E
_
C
O
R
R
E
C
T
N
E
S
S
.
m
d
`
 
的
 
S
C
-
0
0
1
 
行
补
齐
「
实
际
改
动
 
+
 
影
响
面
 
+
 
版
本
递
增
」
，
状
态
改
「
已
执
行
」
；


5
.
 
命
令
带
 
t
i
m
e
o
u
t
，
日
志
落
 
`
r
u
n
/
P
R
O
J
E
C
T
-
G
O
V
E
R
N
A
N
C
E
-
0
1
/
S
C
I
-
F
I
X
-
P
R
O
J
/
l
o
g
s
/
`
。




#
#
 
交
付
与
汇
报
（
中
文
，
直
白
）




1
.
 
逐
条
订
正
表
（
改
前
原
文
 
→
 
改
后
原
文
 
→
 
依
据
）
；
2
.
 
实
现
改
动
与
红
→
绿
证
据
；
3
.
 
锚
点
门
与
 
c
t
e
s
t
 
复
跑
结
果
；
4
.
 
未
做
项
与
原
因
；
5
.
 
自
证
摘
要
。