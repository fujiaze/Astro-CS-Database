'use strict';
// 审计档案锚点自动修复器 v1 —— 只修「纯前缀补全」，其余全转人工复核
const fs=require('fs'), path=require('path');
const AUDIT='问题扫描';
const NL=String.fromCharCode(10), BT=String.fromCharCode(96);
function strip(x){ return x.split(BT).join("").trim(); }
function walk(d,acc){ let e; try{ e=fs.readdirSync(d,{withFileTypes:true}) }catch(x){ return acc; }
  for(const f of e){ if(f.name==='.git') continue; const p=path.join(d,f.name); if(f.isDirectory()) walk(p,acc); else if(f.name.endsWith('.md')) acc.push(p.split(path.sep).join('/')); } return acc; }
const rep=fs.readFileSync(AUDIT+'/_merge/ANCHOR_VERIFY_REPORT.md','utf8').split(NL);
const pairs=[];
for(const line of rep){
  if(!line.startsWith('|')) continue;
  const c=line.split('|'); if(c.length<4) continue;
  const a=strip(c[1]), b=strip(c[2]);
  if(!a||!b) continue;
  if(a.indexOf('/')<0 && a.indexOf('.')<0) continue;
  if(/^-+$/.test(a)) continue;
  if(a==='档案里的写法'||b==='应改为') continue;
  pairs.push([a,b]);
}
const seen=Object.create(null); const uniq=[];
for(const pr of pairs){ const k=pr[0]+'#'+pr[1]; if(seen[k]) continue; seen[k]=1; uniq.push(pr); }
function pureCompletion(a,b){
  if(b===a) return false;
  if(b.length<=a.length) return false;
  if(!b.endsWith(a)) return false;
  if(!b.slice(0,b.length-a.length).endsWith('/')) return false;
  if(a.split('/').length>=b.split('/').length) return false;
  return true;
}
const ALLOW=[['tools/quality/check_api_docs.py','tools/check_api_docs.py'],
 ['docs/standards/DATA_ARTIFACTS.md','docs/contracts/DATA_ARTIFACTS.md'],
 ['docs/science/PHASE2_SAMPLER.md','docs/algorithms/PHASE2_SAMPLER.md'],
 ['docs/contracts/TRACEABILITY.csv','docs/TRACEABILITY.csv']];
const auto=[], manual=[];
for(const pr of uniq){
  const al=ALLOW.some(x=>x[0]===pr[0]&&x[1]===pr[1]);
  if(pureCompletion(pr[0],pr[1])||al) auto.push(pr); else manual.push(pr);
}
const SKIP=['ANCHOR_VERIFY_REPORT.md','10_PROTOCOL.md','anchor_repair_manual.md','ANCHOR_REPAIR_LOG.md'];
const targets=walk(AUDIT, []).filter(p=>!SKIP.some(s2=>p.indexOf(s2)>=0));
const log=[]; let total=0, touched=0;
for(const f of targets){
  let t=fs.readFileSync(f,'utf8'); const before=t; const per=[];
  for(const pr of auto){
    const a=pr[0], b=pr[1]; if(a===b) continue;
    const n=t.split(a).length-1;
    if(n>0){ t=t.split(a).join(b); per.push(a+' -> '+b+' x'+n); total+=n; }
  }
  if(t!==before){ fs.writeFileSync(f,t); touched++; log.push({file:f,edits:per}); }
}
const L=[];
L.push('# 锚点自动修复日志 v1','');
L.push('- 授权：负责人允许在 `问题扫描/` 内跑命令。本脚本**只写 `问题扫描/**`，不触碰真源**。');
L.push('- **规则（保守）**：只自动修**纯前缀补全** —— `good.endsWith(bad)`、补出的前缀以 `/` 结尾、且 bad 段数更少（**信息只增不减**）。');
L.push('- 另有 4 条「同 basename 换目录」由前台逐条签字后进白名单；其余**全部转人工**，绝不自动改。');
L.push('- **为什么必须这么保守**：解析器曾把 `工程控制/…FINAL3/templates/AGENTS.md` 按 basename 唯一命中成 `AGENTS.md`，');
L.push('  自动替换会把一条**正确的特指引用改成另一个文件**（反向劣化）。这类全部落在人工清单里。','');
L.push('## 一、本次自动修复','');
L.push('- 可修对数 **'+auto.length+'** ／ 待人工 **'+manual.length+'**；实际替换 **'+total+'** 处，触及 **'+touched+'** 份档案。','');
L.push('| 档案 | 修的对（次数） |','|---|---|');
for(const e of log) L.push('| '+e.file+' | '+e.edits.join('；').slice(0,280)+' |');
L.push('','## 二、前台签字白名单（同 basename 换目录）','');
for(const x of ALLOW) L.push('- `'+x[0]+'` → `'+x[1]+'`');
L.push('','## 三、待人工复核（未自动改，共 '+manual.length+' 对；全量见 `_tools/anchor_repair_manual.md`）','');
L.push('| 档案里的写法 | 解析器建议 | 为什么不敢自动改 |','|---|---|---|');
for(const pr of manual.slice(0,120)){
  const a=pr[0], b=pr[1]; let why;
  if(b.endsWith(a)) why='补出的前缀不以斜杠结尾（可能是拼写变体而非缺前缀）';
  else if(a.endsWith(b)) why='建议值反而更短或同名他指：可能把正确特指引用改成另一个文件（反向劣化）';
  else why='两侧非前后缀关系：解析器按 basename 撞名，须人工确认是否同一文件';
  L.push('| `'+a+'` | `'+b+'` | '+why+' |');
}
fs.writeFileSync(AUDIT+'/_merge/ANCHOR_REPAIR_LOG.md', L.join(NL)+NL);
fs.writeFileSync(AUDIT+'/_tools/anchor_repair_manual.md', '# 待人工复核的锚点对'+NL+NL+'| 档案写法 | 解析器建议 |'+NL+'|---|---|'+NL+manual.map(x=>'| `'+x[0]+'` | `'+x[1]+'` |').join(NL)+NL);
console.log('可修='+auto.length+' 人工='+manual.length+' 替换处='+total+' 触及文件='+touched);
