// Independent research microprototype, not linked with production ACSD.
// Interval first derivatives enclose a smooth HPX chart by affine strips.
// GMP rational half-plane clipping gives exact polygon area and moments.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <gmpxx.h>
#include <mpfr.h>
#include <omp.h>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
using namespace std;
constexpr double INF=numeric_limits<double>::infinity();
double dn(double x){return nextafter(x,-INF);} double up(double x){return nextafter(x,INF);}
struct I{double l,h; I(double x=0):l(x),h(x){} I(double a,double b):l(a),h(b){}};
I operator+(I a,I b){return {dn(a.l+b.l),up(a.h+b.h)};}
I operator-(I a){return {-a.h,-a.l};} I operator-(I a,I b){return a+(-b);}
I operator*(I a,I b){double v[]={a.l*b.l,a.l*b.h,a.h*b.l,a.h*b.h};return {dn(*min_element(v,v+4)),up(*max_element(v,v+4))};}
I operator/(I a,I b){if(b.l<=0&&b.h>=0)throw 1;return a*I(dn(1/b.h),up(1/b.l));}
double mag(I a){return max(abs(a.l),abs(a.h));} double mid(I a){return a.l+(a.h-a.l)/2;}
bool finite(I a){return isfinite(a.l)&&isfinite(a.h)&&a.l<=a.h;}
double fn(double x,int op,mpfr_rnd_t rnd){mpfr_t a;mpfr_init2(a,80);mpfr_set_d(a,x,MPFR_RNDN);
 if(op==0)mpfr_sqrt(a,a,rnd); if(op==1)mpfr_atan(a,a,rnd);if(op==2)mpfr_sin(a,a,rnd);if(op==3)mpfr_cos(a,a,rnd);
 double v=mpfr_get_d(a,rnd);mpfr_clear(a);return v;}
I root(I a){if(a.l<0)throw 1;return {fn(a.l,0,MPFR_RNDD),fn(a.h,0,MPFR_RNDU)};}
I atanI(I a){return {fn(a.l,1,MPFR_RNDD),fn(a.h,1,MPFR_RNDU)};}
I piI(){mpfr_t p;mpfr_init2(p,80);mpfr_const_pi(p,MPFR_RNDD);double l=mpfr_get_d(p,MPFR_RNDD);mpfr_const_pi(p,MPFR_RNDU);double h=mpfr_get_d(p,MPFR_RNDU);mpfr_clear(p);return {l,h};}
I trig(I a,int op){ // endpoint+critical-point enclosure, valid for all finite input ranges
 if(a.h-a.l>6.0)return {-1,1};I pi=piI();double l=min(fn(a.l,op,MPFR_RNDD),fn(a.h,op,MPFR_RNDD));
 double h=max(fn(a.l,op,MPFR_RNDU),fn(a.h,op,MPFR_RNDU));
 for(int k=-16;k<=16;k++){I c=pi*I(k)/I(2);if(c.h>=a.l&&c.l<=a.h){int r=((k%4)+4)%4;double v=op==2?(r==1?1:r==3?-1:0):(r==0?1:r==2?-1:0);l=min(l,v);h=max(h,v);}}
 // Headers are reduced to [-2pi,2pi]; no trigonometric call has a larger argument.
 if(a.l < -7 || a.h > 7) return {-1,1};return {l,h};}
struct D{I v,x,y;D(I a=I(0),I b=I(0),I c=I(0)):v(a),x(b),y(c){}};
D operator+(D a,D b){return {a.v+b.v,a.x+b.x,a.y+b.y};}D operator-(D a){return {-a.v,-a.x,-a.y};}D operator-(D a,D b){return a+(-b);}
D operator*(D a,D b){return {a.v*b.v,a.x*b.v+a.v*b.x,a.y*b.v+a.v*b.y};}
D operator/(D a,D b){return {a.v/b.v,(a.x*b.v-a.v*b.x)/(b.v*b.v),(a.y*b.v-a.v*b.y)/(b.v*b.v)};}
I squareI(I a){if(a.l>=0)return a*a;if(a.h<=0)return (-a)*(-a);return I(0,up(max(a.l*a.l,a.h*a.h)));}
D sqr(D a){return {squareI(a.v),I(2)*a.v*a.x,I(2)*a.v*a.y};}D sqrtD(D a){I s=root(a.v);return {s,a.x/(I(2)*s),a.y/(I(2)*s)};}
D atanD(D a){I den=I(1)+a.v*a.v;return {atanI(a.v),a.x/den,a.y/den};}
D powD(D a,int n){D r(I(1));for(int k=0;k<n;k++)r=r*a;return r;}
struct Term{int axis,i,j;double c;};
struct Rect{double x0,x1,y0,y1;};
struct Model{double ra,dec,rx,ry,c11,c12,c21,c22;vector<Term> terms;array<I,3> c,e,n;I det,rad;
 void init(){rad=piI()/I(180);I a=I(remainder(ra,360.))*rad,d=I(dec)*rad;I ca=trig(a,3),sa=trig(a,2),cd=trig(d,3),sd=trig(d,2);
 c={cd*ca,cd*sa,sd};e={-sa,ca,I(0)};n={-sd*ca,-sd*sa,cd};det=(I(c11)*I(c22)-I(c12)*I(c21))*rad*rad;}
 struct Native{D X,Y,Z,z,g;};
 Native native(Rect r)const{D u(I(r.x0,r.x1)+I(1)-I(rx),I(1)),v(I(r.y0,r.y1)+I(1)-I(ry),I(0),I(1));
 D a=u,b=v,au(I(1)),av,bu,bv(I(1));
 for(auto t:terms){D val=D(I(t.c))*powD(u,t.i)*powD(v,t.j);if(t.axis==0)a=a+val;else b=b+val;
 if(t.i){D dx=D(I(t.c)*I(t.i))*powD(u,t.i-1)*powD(v,t.j);if(t.axis==0)au=au+dx;else bu=bu+dx;}
 if(t.j){D dy=D(I(t.c)*I(t.j))*powD(u,t.i)*powD(v,t.j-1);if(t.axis==0)av=av+dy;else bv=bv+dy;}}
 D xi=(D(I(c11))*a+D(I(c12))*b)*D(rad),eta=(D(I(c21))*a+D(I(c22))*b)*D(rad);
 D len=sqrtD(D(I(1))+sqr(xi)+sqr(eta));D X=D(c[0])+D(e[0])*xi+D(n[0])*eta,Y=D(c[1])+D(e[1])*xi+D(n[1])*eta,Z=D(c[2])+D(n[2])*eta;
 D jd=au*bv-av*bu; if(jd.v.h<0)jd=-jd;else if(jd.v.l<=0)jd=D(I(0,mag(jd.v)),I(-mag(jd.x),mag(jd.x)),I(-mag(jd.y),mag(jd.y)));
 I absdet=det.l>0?det:-det;D g=D(absdet)*jd/(len*len*len);return {X,Y,Z,Z/len,g};}
};
using Q=mpq_class;
struct Pt{Q x,y;};using Poly=vector<Pt>;
Poly box(Rect r){return {{Q(r.x0),Q(r.y0)},{Q(r.x1),Q(r.y0)},{Q(r.x1),Q(r.y1)},{Q(r.x0),Q(r.y1)}};}
// a*x+b*y+c>=0, exact for the represented binary64 coefficients.
Poly clip(const Poly& p,double aa,double bb,double cc){if(p.empty())return {};Q a(aa),b(bb),c(cc);Poly o;
 auto f=[&](const Pt& t)->Q{return a*t.x+b*t.y+c;};
 for(size_t i=0;i<p.size();i++){Pt s=p[i],t=p[(i+1)%p.size()];Q fs=f(s),ft=f(t);bool is=fs>=0,it=ft>=0;
 if(is)o.push_back(s);if(is!=it){Q k=fs/(fs-ft);o.push_back({s.x+k*(t.x-s.x),s.y+k*(t.y-s.y)});}}return o;}
struct Moments{Q a,x,y;};
Moments moments(const Poly& p){Q a=0,x=0,y=0;for(size_t i=0;i<p.size();i++){const auto&s=p[i];const auto&t=p[(i+1)%p.size()];Q d=s.x*t.y-s.y*t.x;a+=d;x+=(s.x+t.x)*d;y+=(s.y+t.y)*d;}return {a/2,x/6,y/6};}
I qi(const Q&q){double d=q.get_d();return {dn(d),up(d)};}
struct Affine{double c,x,y,err;};
double radius(double lo,double hi){double center=(lo+hi)/2;return max(up(center-lo),up(hi-center));}
Affine affine(D point,D whole,Rect r){double x=mid(point.x),y=mid(point.y),c=mid(point.v);double hx=radius(r.x0,r.x1),hy=radius(r.y0,r.y1);
 I er=(whole.x-I(x))*I(-hx,hx)+(whole.y-I(y))*I(-hy,hy)+(point.v-I(c));
 return {c,x,y,up(mag(er))};}
// Centered affine strips are converted to global coordinates with outward constants.
void constrain(Poly& p,Affine a,Rect r,double lo,double hi,bool inner){double cx=(r.x0+r.x1)/2,cy=(r.y0+r.y1)/2;
 I cc=I(a.c)-I(a.x)*I(cx)-I(a.y)*I(cy);I l=cc-I(lo),h=I(hi)-cc;double e=inner?-a.err:a.err;
 p=clip(p,a.x,a.y,inner?dn(l.l+e):up(l.h+e));p=clip(p,-a.x,-a.y,inner?dn(h.l+e):up(h.h+e));}
I integral(const Poly&p,Affine g,Rect r){if(p.empty())return I(0);Moments m=moments(p);I area=qi(m.a),dx=qi(m.x)-I((r.x0+r.x1)/2)*area,dy=qi(m.y)-I((r.y0+r.y1)/2)*area;
 I ans=I(g.c)*area+I(g.x)*dx+I(g.y)*dy+I(-g.err,g.err)*area;return {max(0.,ans.l),max(0.,ans.h)};}
uint64_t morton(uint32_t x,uint32_t y){uint64_t r=0;for(int i=0;i<29;i++){r|=uint64_t((x>>i)&1)<<(2*i);r|=uint64_t((y>>i)&1)<<(2*i+1);}return r;}
uint64_t eqleaf(int64_t jp,int64_t jm,int N){int64_t ifp=jp/N,ifm=jm/N; // normalize common longitude sheet before divisions
 while(jp<0||jm<0){jp+=int64_t(4)*N;jm+=int64_t(4)*N;}while(jp>=int64_t(5)*N||jm>=int64_t(5)*N){jp-=int64_t(4)*N;jm-=int64_t(4)*N;}
 ifp=jp/N;ifm=jm/N;int64_t face=ifp==ifm?(ifp|4):(ifp<ifm?ifp:ifm+8);face%=12;
 return uint64_t(face)*N*N+morton(jm%N,N-1-jp%N);}
struct Block{Rect r;map<uint64_t,I> areas;double error=0;bool singular=false;int candidates=0;};
struct Stats{uint64_t nodes=0,candidates=0,cache_try=0,cache_accept=0,local_retry=0;size_t max_queue=0;};
struct Domain{Model::Native n;};
Block evaluate(const Model&m,Rect r,int N,const Domain*cache=nullptr,int64_t focus=-1){Block out;out.r=r;
 double cx=(r.x0+r.x1)/2,cy=(r.y0+r.y1)/2;Rect cr{cx,cx,cy,cy};
 try{auto p=m.native(cr),w=cache?cache->n:m.native(r);Affine g=affine(p.g,w.g,r);
 // Longitude chart is centered at a represented direction; rotation is enclosed.
 double alpha=atan2(mid(p.Y.v),mid(p.X.v));I ca=trig(I(alpha),3),sa=trig(I(alpha),2);
 auto phi=[&](const Model::Native&v){D xx=D(ca)*v.X+D(sa)*v.Y,yy=D(-sa)*v.X+D(ca)*v.Y;if(xx.v.l<=0)throw 1;return D(I(alpha))+atanD(yy/xx);};
 D pp=phi(p),pw=phi(w);D tt0=pp/D(piI()/I(2)),tt= pw/D(piI()/I(2));
 auto consume=[&](D a0,D a,D b0,D b,int mode,int sector){
 Affine aa=affine(a0,a,r),bb=affine(b0,b,r),zz=affine(p.z,w.z,r);
 auto extent=[&](Affine q){double hx=radius(r.x0,r.x1),hy=radius(r.y0,r.y1);I v=I(q.c)+I(q.x)*I(-hx,hx)+I(q.y)*I(-hy,hy)+I(-q.err,q.err);return v;};
 I ar=extent(aa),br=extent(bb);if(!finite(ar)||!finite(br)||mag(ar)>1e15||mag(br)>1e15)throw 1;
 int64_t j0=floor(ar.l),j1=floor(ar.h),k0=floor(br.l),k1=floor(br.h);
 if(mode){j0=max(int64_t(0),j0);k0=max(int64_t(0),k0);j1=min(int64_t(N-1),j1);k1=min(int64_t(N-1),k1);}
 if(j1<j0||k1<k0)return;if(j1-j0+1>4096||k1-k0+1>4096/(j1-j0+1))throw 1;
 for(int64_t j=j0;j<=j1;j++)for(int64_t k=k0;k<=k1;k++){
 Poly in=box(r),outer=in;constrain(in,aa,r,j,j+1,true);constrain(outer,aa,r,j,j+1,false);
 constrain(in,bb,r,k,k+1,true);constrain(outer,bb,r,k,k+1,false);
 double zl=mode==1?2./3:mode==-1?-1:-2./3,zh=mode==1?1:2./3;
 if(mode==-1)zh=-2./3;
 // Use interval enclosure of mathematical 2/3, not a rounded threshold.
 I two=I(2)/I(3);double il=zl,ih=zh,ol=zl,oh=zh;
 if(mode==0){il=two.h*-1;ih=two.l;ol=-two.h;oh=two.h;il=-two.l;}
 if(mode==1){il=two.h;ol=two.l;}if(mode==-1){ih=-two.h;oh=-two.l;}
 constrain(in,zz,r,il,ih,true);constrain(outer,zz,r,ol,oh,false);
 if(outer.empty())continue;out.candidates++;
 uint64_t leaf=mode==0?eqleaf(j,k,N):uint64_t((mode==1?0:8)+((sector%4+4)%4))*N*N+morton(mode==1?N-k-1:j,mode==1?N-j-1:k);
 I lower=integral(in,g,r),upper=integral(outer,g,r);I value(max(0.,lower.l),upper.h);
 auto it=out.areas.find(leaf);if(it==out.areas.end())out.areas[leaf]=value;else it->second=it->second+value;
 }};
 if(w.z.v.l<=2./3&&w.z.v.h>=-2./3)consume((D(I(.5))+tt0-D(I(.75))*p.z)*D(I(N)),(D(I(.5))+tt-D(I(.75))*w.z)*D(I(N)),(D(I(.5))+tt0+D(I(.75))*p.z)*D(I(N)),(D(I(.5))+tt+D(I(.75))*w.z)*D(I(N)),0,0);
 for(int sign:{1,-1})if(sign==1?w.z.v.h>=2./3:w.z.v.l<=-2./3){
 auto tau=[&](const Model::Native&v){D L=sqrtD(sqr(v.X)+sqr(v.Y)+sqr(v.Z)),zz=sign==1?v.Z:-v.Z;
 return D(I(N))*sqrtD(D(I(3)))*sqrtD(sqr(v.X)+sqr(v.Y))/sqrtD(L*(L+zz));};
 D t0=tau(p),tw=tau(w);int s0=floor(tt.v.l),s1=floor(tt.v.h);
 if(s1-s0>4)throw 1;
 for(int sec=s0;sec<=s1;sec++){D h0=tt0-D(I(sec)),hw=tt-D(I(sec));consume(h0*t0,hw*tw,(D(I(1))-h0)*t0,(D(I(1))-hw)*tw,sign,sec);}}
 for(auto [ip,a]:out.areas)if(focus<0||ip==uint64_t(focus))out.error=up(out.error+up(a.h-a.l));
 }catch(int){out.singular=true;try{auto w=m.native(r);out.error=((I(r.x1)-I(r.x0))*(I(r.y1)-I(r.y0))*I(max(0.,w.g.v.h))).h;}catch(int){out.error=INF;}}
 return out;}
struct Solution{map<uint64_t,I> areas;double spill=0,width=0,all_width=0;string status;Stats stats;double seconds=0;};
Solution solve(const Model&m,Rect r,int N,double tol,uint64_t maxnodes,const Domain*cache,int64_t focus=-1){auto start=chrono::steady_clock::now();
 struct Cmp{bool operator()(const Block&a,const Block&b){return a.error<b.error;}};priority_queue<Block,vector<Block>,Cmp>queue;vector<Block>done;Stats stats;
 auto add=[&](Rect q,bool initial){Block b=evaluate(m,q,N,initial?cache:nullptr,focus);stats.nodes++;stats.candidates+=b.candidates;
 if(initial&&cache){stats.cache_try++;if(b.error<=tol){stats.cache_accept++;}else{b=evaluate(m,q,N,nullptr,focus);stats.local_retry++;stats.nodes++;stats.candidates+=b.candidates;}}
 double inserted=b.error;queue.push(move(b));stats.max_queue=max(stats.max_queue,queue.size());return inserted;};
 add(r,true);double error=queue.top().error;Q ledger(error);
 while(error>tol&&stats.nodes+4<=maxnodes){Block b=queue.top();queue.pop();double cx=(b.r.x0+b.r.x1)/2,cy=(b.r.y0+b.r.y1)/2;
 if(cx==b.r.x0||cx==b.r.x1||cy==b.r.y0||cy==b.r.y1){done.push_back(move(b));break;}
 ledger-=Q(b.error);
 for(Rect q:vector<Rect>{{b.r.x0,cx,b.r.y0,cy},{cx,b.r.x1,b.r.y0,cy},{b.r.x0,cx,cy,b.r.y1},{cx,b.r.x1,cy,b.r.y1}})ledger+=Q(add(q,false));
 error=up(ledger.get_d());
 }
 Solution s;s.stats=stats;while(!queue.empty()){done.push_back(queue.top());queue.pop();}
 for(auto &b:done){if(b.singular){s.spill=up(s.spill+b.error);continue;}for(auto [ip,a]:b.areas){auto it=s.areas.find(ip);if(it==s.areas.end())s.areas[ip]=a;else it->second=it->second+a;}}
 s.width=s.spill;s.all_width=s.spill;for(auto [ip,a]:s.areas){a.l=max(0.,a.l);s.all_width=up(s.all_width+up(a.h-a.l));if(focus<0||ip==uint64_t(focus))s.width=up(s.width+up(a.h-a.l));}
 s.status=s.width<=tol?"bounded":"budget_exhausted";s.seconds=chrono::duration<double>(chrono::steady_clock::now()-start).count();return s;}
int main(int argc,char**argv){int64_t focus=argc==3&&string(argv[1])=="--leaf"?stoll(argv[2]):-1;cout<<setprecision(17);string name;while(cin>>name){Model m;int N,nt,side,cache,threads;double frac,tol;uint64_t limit;
 cin>>N>>frac>>tol>>limit>>side>>cache>>threads>>m.ra>>m.dec>>m.rx>>m.ry>>m.c11>>m.c12>>m.c21>>m.c22>>nt;
 if(N<1||N>(1<<29)||(N&(N-1))||frac<=0||frac>1||side<1||threads<1||tol<=0||limit<1){cerr<<"invalid research input"<<endl;return 2;}
 for(int k=0;k<nt;k++){Term t;cin>>t.axis>>t.i>>t.j>>t.c;m.terms.push_back(t);}m.init();double offset=-(side-1)/2.;
 Domain tile;bool usecache=cache;try{tile.n=m.native({offset-.5,offset+side-.5,offset-.5,offset+side-.5});}catch(int){usecache=false;}
 vector<Solution> ss(side*side);auto start=chrono::steady_clock::now();
 #pragma omp parallel for num_threads(threads) schedule(static)
 for(int k=0;k<side*side;k++){double x=offset+k%side,y=offset+k/side;ss[k]=solve(m,{x-frac/2,x+frac/2,y-frac/2,y+frac/2},N,tol,limit,usecache?&tile:nullptr,focus);}
 double secs=chrono::duration<double>(chrono::steady_clock::now()-start).count();uint64_t nodes=0,candidates=0,hits=0,retries=0;size_t queue=0,leaves=0;int bounded=0;double worst=0;
 for(auto&s:ss){nodes+=s.stats.nodes;candidates+=s.stats.candidates;hits+=s.stats.cache_accept;retries+=s.stats.local_retry;queue=max(queue,s.stats.max_queue);leaves=max(leaves,s.areas.size());bounded+=s.status=="bounded";worst=max(worst,s.width);}
 size_t rss=0;
 #ifdef _WIN32
 PROCESS_MEMORY_COUNTERS pmc{};if(GetProcessMemoryInfo(GetCurrentProcess(),&pmc,sizeof(pmc)))rss=pmc.PeakWorkingSetSize;
 #endif
 cout<<"{\"name\":\""<<name<<"\",\"pixels\":"<<side*side<<",\"seconds\":"<<secs<<",\"us_per_pixel\":"<<secs*1e6/(side*side)<<",\"nodes\":"<<nodes<<",\"candidate_visits\":"<<candidates<<",\"max_leaves\":"<<leaves<<",\"max_queue\":"<<queue<<",\"peak_process_workingset_bytes\":"<<rss<<",\"cache_accepted_pixels\":"<<hits<<",\"cache_local_retries\":"<<retries<<",\"bounded_pixels\":"<<bounded<<",\"worst_width_sr\":"<<worst<<",\"first\":{\"spill_sr\":"<<ss[0].spill<<",\"status\":\""<<ss[0].status<<"\",\"areas\":{";
 bool first=true;for(auto[ip,a]:ss[0].areas){if(!first)cout<<",";first=false;cout<<"\""<<ip<<"\":["<<max(0.,a.l)<<","<<a.h<<"]";}cout<<"},\"all_leaf_width_sr\":"<<ss[0].all_width<<"},\"focused_leaf\":"<<focus<<"}"<<endl;
 }}
