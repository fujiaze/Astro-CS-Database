const cp=require('child_process'), fs=require('fs'), path=require('path');
const BASE='b32246c4', AUDIT='\u95ee\u9898\u626b\u63cf';
const OKP=['lib/','docs/','cli/','runtime/','providers/','modules/','include/','tests/','ci/','tools/','contracts/','packaging/','cmake/','CMakeLists.txt','VERSION','CMakePresets.json','AGENTS.md','ASTROCS_PROJECT_CONSTITUTION.md','DEPENDENCIES.md','README.md','.github/'];
function g(c){try{return cp.execSync('git -c core.quotepath=off '+c,{encoding:'utf8',maxBuffer:1<<28}).trim()}catch(e){return ''}}
const committed=g('diff --name-only '+BASE+'..HEAD -- .').split('\n').map(s=>s.trim()).filter(Boolean).filter(p=>!p.startsWith(AUDIT));
const dirty=g('status --porcelain').split('\n').filter(Boolean).map(l=>l.replace(/^.{0,3}/,'').trim().replace(/^"|"$/g,'')).filter(p=>p&&!p.startsWith(AUDIT));
const inScope=p=>OKP.some(k=>p===k||p.startsWith(k));
const changed=[...new Set([...committed,...dirty])].filter(inScope);
function walk(d,acc){for(const e of fs.readdirSync(d,{withFileTypes:true})){const p=path.join(d,e.name);if(e.isDirectory())walk(p,acc);else if(e.name.endsWith('.md'))acc.push(p)}return acc}
const texts={};for(const d of walk(AUDIT,[])){try{texts[d.replace(/\\/g,'/')]=fs.readFileSync(d,'utf8')}catch(e){}}
const rows=[];const perDoc={};
for(const f of changed){
  const files=new Set(),ids=new Set();let n=0;
  for(const [d,t] of Object.entries(texts)){
    if(t.indexOf(f)<0)continue;
    let cur='';
    for(const ln of t.split('\n')){
      const m=ln.match(/((?:L\d\d|M\d[a-z]?|F00)-[A-Z]?\d{2,3})/);if(m&&!cur)cur=m[1];
      if(ln.indexOf(f)>=0){n++;const mm=ln.match(/((?:L\d\d|M\d[a-z]?|F00)-[A-Z]?\d{2,3})/g);if(mm)mm.forEach(x=>ids.add(x));files.add(d);perDoc[d]=(perDoc[d]||0)+1}
    }
  }
  if(n>0)rows.push({f,src:(committed.includes(f)?'C':'')+(dirty.includes(f)?(committed.includes(f)?'+W':'W'):'W'),n,files:[...files],ids:[...ids].slice(0,8)});
}
rows.sort((a,b)=>b.n-a.n);
const noRef=changed.filter(f=>!rows.some(r=>r.f===f));
const L=[];
L.push('# R 层工作清单：改动面 × 引用它的问题条目（机械交叉索引）');
L.push('');
L.push('- 基线 SHA：**'+BASE+'**；当前 main：**'+g('rev-parse HEAD')+'**；生成于复验前，**R 层开工前必须重跑刷新**。');
L.push('- 基线后已进入 main 的改动（真源白名单内）：**'+committed.filter(inScope).length+'**；当前工作树脏：**'+dirty.filter(inScope).length+'**；改动面合计 **'+changed.length+'**，被审计档案点名 **'+rows.length+'**，未点名 **'+noRef.length+'**。');
L.push('- 统计口径：只匹配**全路径字符串**（含 path::符号 形式），只统计真源前缀（lib/docs/cli/runtime/providers/modules/include/tests/ci/tools/contracts/packaging/cmake/CMakeLists/VERSION/根 md/.github），排除本审计目录与免报区。');
L.push('');
L.push('## 触发条件（满足后才实跑 R 层，中途只刷新本文件）');
L.push('1. `git rev-parse HEAD` 连续两轮采样不变；且 2. 真源白名单内工作树脏文件数为 0；且 3. 全部 M 代理已交付。');
L.push('');
L.push('## 复验规则（负责人指令，R 代理逐条执行）');
L.push('1. 逐文件比对档案给的锚与当前树实际位置：**一律改为 path::符号**，行号仅作「复验时 N」附注；行变了改行。');
L.push('2. 逐条目重判 bug 现状四态：仍成立 / 已被修复 / 部分修复（只报残余）/ 无法判定。');
L.push('3. 已修复者从 findings/ 移出并记入该域 _merge「已修复」表；**修复但无回归测试**另立 F_TEST_GAP。');
L.push('4. 凡条目引用代码当前行为（注释摘录、常量、阈值、默认值、返回值、分支）必须逐字重读重抄，禁止只改行号不改判断。');
L.push('5. 表 B 是**潜在漏报面**：被改动但审计未点名，须抽查是否引入新问题（尤其 lib/ 与 docs/standards、ci/checks.json）。');
L.push('');
L.push('## 表 A：必须复验的改动文件（按被引用次数降序，全列）');
L.push('');
L.push('| 改动文件 | 来源 | 被引 | 涉及条目 |');L.push('|---|---|---|---|');
for(const x of rows)L.push('| '+x.f+' | '+x.src+' | '+x.n+' | '+x.ids.join(', ')+' |');
L.push('');
L.push('## 表 B：被改动但审计档案未点名（潜在漏报面）');L.push('');
for(const f of noRef)L.push('- '+f+(committed.includes(f)?'（已进提交）':'（工作树脏）'));
L.push('');
L.push('## 表 C：按复验主体（档案）分布的工作量');L.push('');
L.push('| 审计档案 | 引用改动面次数 |');L.push('|---|---|');
for(const [d,n] of Object.entries(perDoc).sort((a,b)=>b[1]-a[1]))L.push('| '+d+' | '+n+' |');
fs.writeFileSync(AUDIT+'/_merge/CHANGED_FILES_WATCH.md',L.join('\n')+'\n','utf8');
console.log('changed='+changed.length+' referenced='+rows.length+' unreferenced='+noRef.length);
console.log(rows.slice(0,12).map(x=>x.f+'='+x.n).join('\n'));
