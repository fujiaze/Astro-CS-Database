import re, os
DD='/workspace/Astro CS Database/设计大纲'
A=open(os.path.join(DD,'大报告_项目历史.md'),encoding='utf-8').read()
B=open(os.path.join(DD,'大报告_历代控制包.md'),encoding='utf-8').read()
def cn(t): return len([c for c in t if chr(0x4e00)<=c<=chr(0x9fff)])
print('A 中文字 %d 行 %d；B 中文字 %d 行 %d' % (cn(A), A.count(chr(10)), cn(B), B.count(chr(10))))
for n,t in (('A',A),('B',B)):
    dmg=[]
    dmg.append(('空括号', len(re.findall('（）|\\(\\)', t))))
    dmg.append(('连续终止符', len(re.findall('[。，、；：]{2,}', t))))
    dmg.append(('句末悬空逗号', len(re.findall('[，、：]\\s*$', t, re.M))))
    dmg.append(('粘连_路径+命令', len(re.findall('[a-z_]{4,}(?:git|python3|grep|wc|sed|unzip)[a-z_]', t))))
    dmg.append(('括号连引）（', len(re.findall('）（', t))))
    dmg.append(('双空格', len(re.findall('  +', t))))
    dmg.append(('未闭合书名号', t.count('《')-t.count('》')))
    print(n, dmg)
print()
print('A 是否提到 控制包/大报告B:', len(re.findall('大报告_历代控制包|控制包大报告', A)), '处；B 是否提到 历史/大报告A:', len(re.findall('大报告_项目历史|历史大报告|任务一', B)), '处')
sa=set(re.findall('STAGE-(0\d)', A)); sb=set(re.findall('STAGE-(0\d)', B))
print('A 覆盖阶段:', sorted(sa), 'B 覆盖代际:', sorted(sb))
print('B 是否含 49 身份/74 实例口径:', '49' in B and '74' in B, '；A 是否含 1988/13 根口径:', '1988' in A and '13 根' in A)
print('B 七条主线小节标记:', re.findall('主线[一二三四五六七]', B)[:8])
print('A 六条主线:', re.findall('主线[1-6]|（[1-6]）', A)[:8])
