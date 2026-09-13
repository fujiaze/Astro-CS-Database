// 审计档案锚点机械核验器 v3（只读，不跑构建/测试/git）
// v3 变更（由 M7 的自我否证驱动）：
//  1) 区分「精确路径不存在但 basename 唯一命中」(PATH_MISMATCH，给改锚建议) 与「真缺失」(ABSENT) —— v2 会把前者静默放过
//  2) 符号 0 命中时再查该符号是否在**其他**真源文件出现（SYMBOL_ELSEWHERE = 定义在他处，不该撤条）vs 全仓 0 命中（SYMBOL_ABSENT = 最强假阳/撤条信号）
//  3) 对 ABSENT 给近似名候选（token 重叠），供三级复核起点；并写明：判 MISSING 必须以存在性为准，字面串 0 命中只说明用词不同
'use strict';
const fs = require('fs'), path = require('path');
const AUDIT = '问题扫描';
const EXEMPT = ['run/','build/','out/','artifacts/','evidence/','reports/','工程控制/','GaiaDR3','GaiaDR3SP','BASS','__pycache__','.pytest_cache', AUDIT+'/'];
function walk(d,acc){ let e; try{ e=fs.readdirSync(d,{withFileTypes:true}) }catch(x){ return acc }
  for(const f of e){ if(f.name==='.git') continue; const p=path.join(d,f.name); if(f.isDirectory()) walk(p,acc); else acc.push(p.split(path.sep).join('/')); } return acc; }
const repo = walk('.',[]).map(p=>p.replace(/^\.\//,''));
const real = repo.filter(p=>!EXEMPT.some(e=>p.startsWith(e)) && !/\.(o|d|obj|so|a)$/i.test(p));
const realSet = new Set(real);
const byBase = {};
for(const p of real){ const b=path.basename(p); (byBase[b]=byBase[b]||[]).push(p); }
const lc={},tc={};
function nlines(r){ if(lc[r]!==undefined) return lc[r]; let n=-1; try{ n=fs.readFileSync(r,'utf8').split('\n').length }catch(x){} return lc[r]=n; }
function text(r){ if(tc[r]!==undefined) return tc[r]; let t=''; try{ t=fs.readFileSync(r,'utf8') }catch(x){} return tc[r]=t; }
const symIdx = {}; // symbol -> files
function symFiles(sym){ if(symIdx[sym]) return symIdx[sym]; const out=[]; for(const p of real){ if(!/\.(c|h|cpp|hpp|cc|cxx|py|js|ts|cmake|txt|json|yml|yaml|md)$/i.test(p)) continue; if(text(p).indexOf(sym)>=0) out.push(p); if(out.length>6) break; } return symIdx[sym]=out; }
function near(t){ const b=path.basename(t).replace(/\.(\w+)$/,''); const toks=b.split(/[^\w]+/).filter(x=>x.length>2); const cands=[];
  for(const p of real){ const pb=path.basename(p); if(pb===path.basename(t)) continue; let s=0; for(const k of toks){ if(pb.toLowerCase().includes(k.toLowerCase())) s++; } if(s>=1 && s>=Math.max(1,toks.length-1)) cands.push([s,p]); if(cands.length>40) break; }
  cands.sort((a,b)=>b[0]-a[0]); return cands.slice(0,3).map(x=>x[1]); }
const EXT='jsonl|tsv|csv|cpp|hpp|md|txt|yaml|yml|cmake|ps1|sh|ini|json|py|c|h';
const SEG='[A-Za-z0-9_.\\-\\u4e00-\\u9fff]+';
const RE=new RegExp('(?:'+SEG+'\\/)+'+SEG+'\\.(?:'+EXT+')(?::[0-9]{1,7})?','g');
const RESYM=new RegExp('[A-Za-z0-9_.\\-/]+\\.(?:'+EXT+')(?::[0-9]{1,7})?::([A-Za-z_][A-Za-z0-9_]{2,})','g');
const docs=walk(AUDIT,[]).filter(p=>p.endsWith('.md'));
const mism=[],absent=[],oob=[],symElse=[],symAbs=[]; const seen=new Set(); let refs=0, exact=0;
for(const doc of docs){ const lines=fs.readFileSync(doc,'utf8').split('\n');
  lines.forEach((ln,i)=>{ let m; const re=new RegExp(RE.source,'g');
    while((m=re.exec(ln))){ const tok=m[0]; let file=tok,line=null;
      const ci=tok.indexOf(':'); if(ci>0){ file=tok.slice(0,ci); const r2=tok.slice(ci+1).match(/^([0-9]{1,7})/); if(r2) line=+r2[1]; }
      if(EXEMPT.some(p=>file.startsWith(p))) continue; refs++;
      let rel=null;
      if(realSet.has(file)||(!file.startsWith(AUDIT+'/')&&fs.existsSync(file)&&fs.statSync(file).isFile())) { rel=file; exact++; }
      else { const c=byBase[path.basename(file)]||[]; if(c.length===1) rel=c[0]; else if(c.length>1){ const q=c.filter(x=>x.endsWith(file)); if(q.length===1) rel=q[0]; } }
      if(!rel){ const k='A|'+file; if(!seen.has(k)){ seen.add(k); absent.push({file:file,at:doc+':'+(i+1),cands:near(file)}); } continue; }
      if(rel!==file){ const k='W|'+file+'>'+rel; if(!seen.has(k)){ seen.add(k); mism.push({bad:file,good:rel,at:doc+':'+(i+1)}); } }
      if(line){ const n=nlines(rel); if(n>0&&line>n){ const k='O|'+rel+':'+line; if(!seen.has(k)){ seen.add(k); oob.push({ref:rel+':'+line,n:n,at:doc+':'+(i+1)}); } } } }
    RESYM.lastIndex=0;
    while((m=RESYM.exec(ln))){ const full=m[0],sym=m[1]; const fpart=full.split('::')[0].replace(/:[0-9]+$/,'');
      let rel=realSet.has(fpart)?fpart:null; if(!rel){ const c=byBase[path.basename(fpart)]||[]; if(c.length===1) rel=c[0]; }
      if(!rel) continue; refs++;
      if(text(rel).indexOf(sym)<0){ const others=symFiles(sym).filter(p=>p!==rel);
        const k='S|'+full; if(seen.has(k)) continue; seen.add(k);
        if(others.length) symElse.push({ref:full,at:doc+':'+(i+1),where:others.slice(0,3).join(' ')});
        else symAbs.push({ref:full,at:doc+':'+(i+1)}); } }
  }); }
const L=[];
L.push('# 锚点机械核验报告 v3');
L.push("");
L.push('- 生成器：_tools/verify_anchors.js（v3）；复跑 node 问题扫描/_tools/verify_anchors.js');
L.push("- 本次扫描 "+docs.length+" 份审计 md、"+refs+" 次引用：精确在位 "+exact+" 次；分五类，处置强度不同");
L.push('- **v3 判据（由 M7 自我否证确立）**：判「路径缺失」**必须以存在性检索为准**；某个字面串 0 命中只说明**用词不同**，不是文件不存在。');
L.push('  因此：一、PATH_MISMATCH **不得撤条**，只改路径写法；三、SYMBOL_ELSEWHERE 多半是「定义在他处/字段名不同」，**先补文件再定档**；只有二、ABSENT 与四、SYMBOL_ABSENT 才走撤条或重锚。');
L.push('');
L.push('## 一、路径写错但同名文件唯一存在（'+mism.length+' 种）→ 只改锚，不撤证据');
L.push('| 档案里的写法 | 应改为 | 首次出现 |'); L.push('|---|---|---|');
for(const x of mism.slice(0,400)) L.push('| '+x.bad+' | '+x.good+' | '+x.at+' |');
L.push('');
L.push('## 二、真源中确无此路径（'+absent.length+' 种）→ 需三级复核后改述或撤条');
L.push('| 引用 | 近似名候选 | 首次出现 |'); L.push('|---|---|---|');
for(const x of absent.slice(0,400)) L.push('| '+x.file+' | '+(x.cands.join(' ')||'（无同名近似）')+' | '+x.at+' |');
L.push('');
L.push('## 三、符号在被引文件 0 命中、但在其他真源文件存在（'+symElse.length+' 种）→ 补文件锚点即可，禁止据此撤条');
L.push('| 引用 | 实际所在 | 首次出现 |'); L.push('|---|---|---|');
for(const x of symElse.slice(0,200)) L.push('| '+x.ref+' | '+x.where+' | '+x.at+' |');
L.push('');
L.push('## 四、符号全仓 0 命中（'+symAbs.length+' 种）→ 最强假阳/失效信号，逐条走整句→关键词→定点 read');
L.push('| 引用 | 首次出现 |'); L.push('|---|---|');
for(const x of symAbs.slice(0,200)) L.push('| '+x.ref+' | '+x.at+' |');
L.push('');
L.push('## 五、行号越过文件当前末尾（'+oob.length+' 种）→ 重取行号或改 path::符号');
L.push('| 引用 | 现行数 | 首次出现 |'); L.push('|---|---|---|');
for(const x of oob.slice(0,200)) L.push('| '+x.ref+' | '+x.n+' | '+x.at+' |');
fs.mkdirSync(AUDIT+'/_merge',{recursive:true});
fs.writeFileSync(AUDIT+'/_merge/ANCHOR_VERIFY_REPORT.md', L.join('\n')+'\n');
console.log('docs='+docs.length+' refs='+refs+' exact='+exact+' mismatch='+mism.length+' absent='+absent.length+' sym_elsewhere='+symElse.length+' sym_absent='+symAbs.length+' oob='+oob.length);
console.log('MISMATCH_HEAD:'); console.log(mism.slice(0,10).map(x=>x.bad+' -> '+x.good).join('\n'));
console.log('ABSENT_HEAD:'); console.log(absent.slice(0,10).map(x=>x.file+' [[' + x.cands.join(',') + ']]').join('\n'));
