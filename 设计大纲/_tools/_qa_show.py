import re
B=open('/workspace/Astro CS Database/设计大纲/大报告_历代控制包.md',encoding='utf-8').read()
A=open('/workspace/Astro CS Database/设计大纲/大报告_项目历史.md',encoding='utf-8').read()
for n,t in (('B',B),('A',A)):
    for m in re.finditer('[a-z_]{4,}(?:git|python3|grep|wc|sed|unzip)[a-z_]', t):
        print(n,'粘连候选:', t[max(0,m.start()-40):m.end()+30].replace(chr(10),' '))
    for m in re.finditer('[。，、；：]{2,}', t):
        print(n,'连续终止符:', repr(t[max(0,m.start()-45):m.end()+25]))
