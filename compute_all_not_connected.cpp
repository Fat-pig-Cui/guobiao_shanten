#include "shanten_core.hpp"
#include <unordered_map>
using namespace std; using U=U128;
string ustr(U x){if(!x)return"0";string s;while(x){s+=char('0'+x%10);x/=10;}reverse(s.begin(),s.end());return s;}
struct K{uint8_t n,h;uint64_t g;bool operator==(K const&o)const{return n==o.n&&h==o.h&&g==o.g;}};
struct H{size_t operator()(K const&x)const{return uint64_t(x.n)^(uint64_t(x.h)<<8)^(x.g*0x9e3779b97f4a7c15ULL);}};using M=unordered_map<K,U,H>;
M suit(int max,int su){M o;array<uint8_t,9>c{};int p=1;for(int i=0;i<9;i++)p*=5;for(int code=0;code<p;code++){int z=code,n=0,w=1,g[3]={};for(int i=0;i<9;i++){c[i]=z%5;z/=5;n+=c[i];w*=int(choose_u128(4,c[i]));g[i%3]+=c[i]>0;}if(n<=max){uint64_t mask=0;for(int j=0;j<3;j++)mask|=uint64_t(g[j])<<(4*(3*su+j));o[K{(uint8_t)n,0,mask}]+=w;}}return o;}
M hon(int max){M o;for(int code=0;code<78125;code++){int z=code,n=0,h=0,w=1;for(int i=0;i<7;i++){int c=z%5;z/=5;n+=c;h+=c>0;w*=int(choose_u128(4,c));}if(n<=max)o[K{(uint8_t)n,(uint8_t)h,0}]+=w;}return o;}
M conv(M const&a,M const&b,int max){M o;o.reserve(a.size()*2);for(auto &[x,w]:a)for(auto &[y,v]:b){int n=x.n+y.n;if(n>max)continue;uint64_t g=x.g+y.g;if(g>0xffffffffffffffULL)continue; o[K{(uint8_t)n,(uint8_t)(x.h+y.h),g}]+=w*v;}return o;}
int best(K const&k){int b=0;for(int a=0;a<3;a++)for(int c=0;c<3;c++)if(c!=a)for(int d=0;d<3;d++)if(d!=a&&d!=c){int x=0;int gg[3]={a,c,d};for(int su=0;su<3;su++)x+=(k.g>>(4*(3*su+gg[su])))&15;b=max(b,x+min<int>(k.h,14-x));}return b;}
int main(int argc,char**argv){int lim=argc>1?atoi(argv[1]):37;if(lim<14||lim>54){cerr<<"max N must be 14..54 (U128 range)\n";return 1;}M all;for(int s=0;s<3;s++){auto a=suit(lim,s);cerr<<"suit "<<s<<" "<<a.size()<<"\n";all=all.empty()?std::move(a):conv(all,a,lim);cerr<<"partial "<<all.size()<<"\n";}all=conv(all,hon(lim),lim);cerr<<"all "<<all.size()<<"\n";for(int n=14;n<=lim;n++){array<U,15> hh{};for(auto &[k,w]:all)if(k.n==n){int r=13-best(k);if(r>=-1&&r<=13)hh[r+1]+=w;}cout<<n;for(int r=-1;r<=13;r++)cout<<" "<<ustr(hh[r+1]);cout<<"\n";}}
