# 任务：SCI-FIX-NOISE 噪声/SNR 科学订正（R-5 结论落地）

状态：NOT_STARTED
层：L1　依赖：P1-002（NumPy Oracle 已入库）
文件域互斥组：S7-A（科学文档订正 + 对应实现/门）

#
#
 
依
据




负
责
人
指
令
同
上
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
5
_
噪
声
S
N
R
与
统
计
口
径
.
m
d
`
（
9
3
0
 
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
5
/
e
x
p
1
.
.
e
x
p
9
`
（
可
复
跑
）
。


*
*
结
论
*
*
：
生
产
实
现
里
的
 
`
m
i
n
_
s
a
m
p
l
e
s
=
6
4
`
 
*
*
是
对
的
*
*
，
S
C
I
 
文
档
里
的
 
5
 
是
旧
稿
数
字
（
N
=
5
 
时
单
 
p
a
t
c
h
 
偏
差
 
−
1
9
.
2
%
、
S
C
I
 
自
设
 
5
%
 
o
r
a
c
l
e
 
通
过
率
仅
 
0
.
6
%
；
N
=
6
4
 
为
 
−
1
.
2
5
%
/
9
2
.
8
%
；
权
重
场
误
差
 
5
:
0
.
0
6
8
6
 
v
s
 
6
4
:
0
.
0
1
0
5
）
。




#
#
 
要
订
正
的
（
文
档
为
主
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
 
`
m
i
n
_
s
a
m
p
l
e
s
`
 
默
认
值
 
*
*
5
 
→
 
6
4
*
*
 
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
N
O
I
S
E
_
M
O
D
E
L
.
m
d
 
§
4
`
 
|


|
 
2
 
|
 
i
v
a
r
 
单
位
 
`
p
i
x
e
l
⁻
²
·
A
D
U
⁻
²
`
 
→
 
`
A
D
U
⁻
²
`
（
G
L
O
S
S
A
R
Y
:
1
1
 
明
文
禁
两
套
定
义
）
 
|
 
`
N
O
I
S
E
_
M
O
D
E
L
.
m
d
 
§
2
:
1
6
`
 
|


|
 
3
 
|
 
兜
底
式
记
号
 
`
m
a
x
(
v
m
e
d
_
o
r
_
s
i
g
²
,
 
f
l
o
o
r
)
`
 
拆
为
两
条
显
式
式
（
`
M
7
-
A
-
2
0
3
`
 
原
判
据
对
象
写
错
）
 
|
 
`
N
O
I
S
E
_
M
O
D
E
L
.
m
d
 
§
5
:
5
1
`
 
|


|
 
4
 
|
 
*
*
删
除
权
威
倒
置
表
述
*
*
「
不
改
 
S
C
I
，
以
代
码
为
准
」
 
|
 
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
N
O
I
S
E
_
E
S
T
I
M
A
T
I
O
N
.
m
d
 
§
1
3
.
2
:
1
5
1
-
1
5
3
`
 
|


|
 
5
 
|
 
补
「
`
s
i
g
m
a
_
c
a
l
_
r
e
l
`
 
是
逐
星
散
度
，
标
准
误
 
=
 
1
.
2
5
3
·
σ
/
√
N
」
（
代
码
已
实
现
，
只
补
文
档
）
 
|
 
`
N
O
I
S
E
_
M
O
D
E
L
.
m
d
`
 
|




#
#
 
可
独
立
落
地
的
小
改
（
代
码
；
各
带
红
→
绿
）




-
 
`
M
3
-
A
-
0
0
5
`
：
共
线
/
近
共
线
几
何
判
据
改
*
*
相
对
条
件
数
*
*
 
+
 
b
u
i
l
d
 
阶
段
回
退
 
f
l
a
g
（
现
 
`
d
e
t
=
0
`
 
仍
报
 
`
h
a
s
_
s
p
a
t
i
a
l
_
f
i
e
l
d
=
1
`
，
近
共
线
把
 
v
a
r
i
a
n
c
e
 
场
外
推
到
 
±
6
2
%
）
；


-
 
`
M
3
-
A
-
0
0
6
`
：
两
处
注
释
漏
 
`
/
g
a
i
n
²
`
；
单
测
 
f
i
x
t
u
r
e
 
的
增
益
方
向
与
 
S
C
I
 
式
相
反
（
差
 
g
a
i
n
²
=
2
.
2
5
）
、
容
差
 
±
1
5
%
 
v
s
 
S
C
I
 
5
%
；
第
三
套
常
数
 
1
.
4
8
2
6
 
统
一
；


-
 
`
M
6
a
-
D
-
0
0
7
`
：
`
S
N
R
_
Q
F
_
P
S
F
_
O
K
 
⇔
 
s
t
a
t
u
s
=
=
0
`
（
删
掉
 
o
r
c
h
e
s
t
r
a
t
o
r
 
的
 
`
|
|
=
=
3
`
，
让
未
收
敛
 
P
S
F
 
拿
 
0
.
5
 
而
非
满
权
 
1
.
0
）
；


-
 
`
V
1
2
-
N
-
1
6
`
：
两
个
 
`
k
L
n
1
0
`
 
字
面
量
合
一
（
d
o
u
b
l
e
 
下
逐
位
相
同
）
。




#
#
 
O
r
a
c
l
e
 
独
立
性
补
强
（
P
1
-
0
0
2
 
的
 
N
u
m
P
y
 
O
r
a
c
l
e
 
只
算
"
实
现
回
归
对
拍
"
）




1
.
 
常
数
不
抄
实
现
，
用
 
`
1
/
N
o
r
m
a
l
D
i
s
t
(
)
.
i
n
v
_
c
d
f
(
0
.
7
5
)
`
 
现
算
；


2
.
 
不
要
再
逐
字
段
覆
盖
 
`
d
e
f
a
u
l
t
_
c
o
n
f
i
g
`
（
对
 
5
↔
6
4
 
无
区
分
力
）
；


3
.
 
用
例
 
≥
3
 
且
必
须
覆
盖
*
*
掩
膜
 
r
e
g
i
m
e
*
*
（
p
a
t
c
h
 
残
余
样
本
 
5
~
6
3
）
。




#
#
 
只
登
记
不
改
（
需
负
责
人
单
独
批
）




默
认
掩
膜
 
`
r
m
a
x
=
6
0
p
x
`
 
使
 
2
5
6
²
 
帧
仅
 
5
0
 
颗
星
即
*
*
整
帧
退
化
*
*
 
`
r
c
=
1
`
（
P
h
a
s
e
2
 
权
重
全
失
）
—
—
动
它
要
改
 
`
S
C
I
 
§
6
:
6
7
`
 
的
冻
结
条
款
，
本
任
务
*
*
只
登
记
*
*
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
；


2
.
 
文
档
订
正
后
复
跑
锚
点
门
 
`
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
 
r
c
=
0
；


3
.
 
`
n
i
n
j
a
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
1
s
n
r
|
p
1
n
o
i
s
e
|
p
1
_
n
o
i
s
e
|
n
o
i
s
e
'
`
 
全
绿
；
四
处
小
改
各
给
「
改
前
红
→
改
后
绿
」
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
2
 
行
补
齐
实
际
改
动
并
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
N
O
I
S
E
/
l
o
g
s
/
`
；


6
.
 
若
发
现
文
档
里
还
有
别
的
*
*
无
据
数
字
*
*
，
一
并
列
出
（
不
擅
自
改
，
列
清
单
给
前
台
）
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
 
→
 
改
后
 
→
 
依
据
）
；
2
.
 
四
处
小
改
的
红
→
绿
证
据
；
3
.
 
O
r
a
c
l
e
 
独
立
性
补
强
证
据
；
4
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
 
结
果
；
5
.
 
无
据
数
字
清
单
；
6
.
 
自
证
摘
要
。