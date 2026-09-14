'use strict';
// 锚点自动修复器 v2 —— 先保护已正确的长写法，再做带边界判定的替换（v1 因二次污染被回滚）
const fs=require('fs'), path=require('path');
const AUDIT='问题扫描';
const NL=String.fromCharCode(10), BT=String.fromCharCode(96);
const OKC=String.fromCharCode(1,2,3);
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
for(const pr of pairs){ const k=pr[0]+String.fromCharCode(35)+pr[1]; if(seen[k]) continue; seen[k]=1; uniq.push(pr); }
function pureCompletion(a,b){
  if(b.length<=a.length) return false;
  if(!b.endsWith(a)) return false;
  if(!b.slice(0,b.length-a.length).endsWith("/")) return false;
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
const PATHCH=/[A-Za-z0-9._\-\/\+]/;
// 带边界的替换：出现位置的前一字符不得是路径字符（否则是更长路径的子串）
function rep2(text, a, b){
  let out="", i=0, n=0;
  while(true){
    const j=text.indexOf(a,i);
    if(j<0){ out+=text.slice(i); break; }
    const prev = j>0 ? text.charAt(j-1) : "";
    const k = j + a.length;
    const next = k < text.length ? text.charAt(k) : "";
    const badPrev = prev !== "" && PATHCH.test(prev);
    const badNext = next !== "" && PATHCH.test(next);
    if(!badPrev && !badNext){ out+=text.slice(i,j)+b; n++; } else { out+=text.slice(i,k); }
    i = k;
  }
  return [out,n];
}
const SKIP=['ANCHOR_VERIFY_REPORT.md','10_PROTOCOL.md','anchor_repair_manual.md','ANCHOR_REPAIR_LOG.md','anchor_repair.js'];
const targets=walk(AUDIT, []).filter(p=>!SKIP.some(x=>p.indexOf(x)>=0));
const log=[]; let total=0, touched=0;
for(const f of targets){
  let t=fs.readFileSync(f,'utf8'); const before=t; const per=[];
  for(const pr of auto){
    const a=pr[0], b=pr[1]; if(a===b) continue;
    t=t.split(b).join(OKC);
    const res=rep2(t,a,b); t=res[0];
    if(res[1]>0){ per.push(a+" -> "+b+" x"+res[1]); total+=res[1]; }
    t=t.split(OKC).join(b);
  }
  if(t!==before){ fs.writeFileSync(f,t); touched++; log.push({file:f,edits:per}); }
}
const L=[];
L.push('# 锚点自动修复日志（v2）','');
L.push('- v1 曾把短路径当作长路径的子串二次替换（mismatch 340→530），**已整体回滚**；v2 加了两道防护：替换前先保护已正确的长写法，替换时要求前后字符不是路径字符。');
L.push('- 可修对数 **'+auto.length+'** ／ 待人工 **'+manual.length+'**；实际替换 **'+total+'** 处，触及 **'+touched+'** 份档案。','');
L.push('| 档案 | 处数 |','|---|---|');
for(const e of log) L.push('| '+e.file+' | '+e.edits.length+' |');
L.push('','## 待人工复核前 60 对（未自动改，全量见 `_tools/anchor_repair_manual.md`）','');
L.push('| 档案写法 | 解析器建议 |','|---|---|');
for(const pr of manual.slice(0,60)) L.push('| `'+pr[0]+'` | `'+pr[1]+'` |');
fs.writeFileSync(AUDIT+'/_merge/ANCHOR_REPAIR_LOG.md', L.join(NL)+NL);
fs.writeFileSync(AUDIT+'/_tools/anchor_repair_manual.md', '# 待人工复核的锚点对（'+manual.length+' 对）'+NL+NL+'| 档案写法 | 解析器建议 |'+NL+'|---|---|'+NL+manual.map(x=>'| `'+x[0]+'` | `'+x[1]+'` |').join(NL)+NL);
console.log('可修='+auto.length+' 人工='+manual.length+' 替换处='+total+' 触及文件='+touched);
