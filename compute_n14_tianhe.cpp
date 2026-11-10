#include <iostream>
#include <vector>
#include <array>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <cstdint>
using namespace std; using A=array<unsigned char,34>;
struct H{size_t operator()(A const& a)const noexcept{size_t h=1469598103934665603ULL;for(auto x:a)h=(h^x)*1099511628211ULL;return h;}};
vector<A> groups;unordered_set<A,H> stdset,combset; long long wt(A const& c){static int C[5]={1,4,6,4,1};long long w=1;for(int x:c)w*=C[x];return w;}
void gen(int at,int left,A& c){if(!left){for(int p=0;p<34;p++)if(c[p]<=2){A d=c;d[p]+=2;stdset.insert(d);}return;}for(int g=at;g<(int)groups.size();g++){bool ok=1;for(int i=0;i<34;i++)if(c[i]+groups[g][i]>4){ok=0;break;}if(!ok)continue;for(int i=0;i<34;i++)c[i]+=groups[g][i];gen(g,left-1,c);for(int i=0;i<34;i++)c[i]-=groups[g][i];}}
int main(){for(int s=0;s<3;s++)for(int st=0;st<7;st++){A a{};for(int k=0;k<3;k++)a[s*9+st+k]=1;groups.push_back(a);}for(int i=0;i<34;i++){A a{};a[i]=3;groups.push_back(a);}A z{};gen(0,4,z);long long nstd=0;for(auto const& x:stdset)nstd+=wt(x);cout<<"N_STANDARD "<<nstd<<" vectors "<<stdset.size()<<"\n";
 long long n7=0,n7std=0;vector<int> q; unordered_set<A,H> sevenSet;function<void(int,int,int)>fq=[&](int at,int left,int nq){if(!left){vector<char>u(34);for(int x:q)u[x]=1;vector<int> rem;for(int i=0;i<34;i++)if(!u[i])rem.push_back(i);vector<int>ps;function<void(int,int)>fp=[&](int a,int l){if(!l){A c{};for(int x:q)c[x]=4;for(int x:ps)c[x]=2;long long w=wt(c);n7+=w;sevenSet.insert(c);if(stdset.count(c))n7std+=w;return;}for(int j=a;j<=((int)rem.size()-l);j++){ps.push_back(rem[j]);fp(j+1,l-1);ps.pop_back();}};fp(0,7-2*nq);return;}for(int i=at;i<=34-left;i++){q.push_back(i);fq(i+1,left-1,nq);q.pop_back();}};for(int nq=0;nq<=3;nq++)fq(0,nq,nq);cout<<"N_7PAIRS "<<n7<<" INTER "<<n7std<<"\n";
 long long n13=13LL*6*(1LL<<24);cout<<"N_13 "<<n13<<"\n";
 int cond[6][9]={{0,3,6,10,13,16,20,23,26},{0,3,6,11,14,17,19,22,25},{1,4,7,11,14,17,18,21,24},{1,4,7,9,12,15,20,23,26},{2,5,8,9,12,15,19,22,25},{2,5,8,10,13,16,18,21,24}};vector<A>all;
 for(int a0=0;a0<4;a0++)for(int a1=0;a1<4;a1++)for(int a2=0;a2<4;a2++){int aa[3]={a0-1,a1-1,a2-1};bool ok=1;for(int i=0;i<3;i++)if(aa[i]>=0)for(int j=0;j<i;j++)if(aa[j]==aa[i])ok=0;if(!ok)continue;for(int m0=0;m0<8;m0++)for(int m1=0;m1<8;m1++)for(int m2=0;m2<8;m2++){int ms[3]={m0,m1,m2};if((aa[0]<0&&m0)||(aa[1]<0&&m1)||(aa[2]<0&&m2))continue;vector<int>ss;for(int s=0;s<3;s++)if(aa[s]>=0)for(int k=0;k<3;k++)if(ms[s]>>k&1)ss.push_back(s*9+aa[s]+3*k);int h=14-(int)ss.size();if(h<0||h>7)continue;for(int hm=0;hm<128;hm++)if(__builtin_popcount((unsigned)hm)==h){A c{};for(int x:ss)c[x]=1;for(int k=0;k<7;k++)if(hm>>k&1)c[27+k]=1;all.push_back(c);}}}
sort(all.begin(),all.end());all.erase(unique(all.begin(),all.end()),all.end());long long nall=0;for(auto const&x:all)nall+=wt(x);cout<<"N_ALLNOT "<<nall<<" vectors "<<all.size()<<"\n";
 for(int r=0;r<6;r++)for(auto &g:groups)for(int p=0;p<34;p++){A c{};for(int j=0;j<9;j++)c[cond[r][j]]++;for(int i=0;i<34;i++)c[i]+=g[i];c[p]+=2;bool ok=1;for(int i=0;i<34;i++)if(c[i]>4)ok=0;if(ok)combset.insert(c);}for(auto const&x:all){for(int r=0;r<6;r++){bool yes=1;for(int j=0;j<9;j++)if(!x[cond[r][j]])yes=0;if(yes){combset.insert(x);break;}}}long long ncomb=0,ncombstd=0,ncomball=0,ncombseven=0;for(auto const&x:combset){long long w=wt(x);ncomb+=w;if(stdset.count(x))ncombstd+=w;if(sevenSet.count(x))ncombseven+=w;if(binary_search(all.begin(),all.end(),x))ncomball+=w;}cout<<"N_COMBO "<<ncomb<<" vectors "<<combset.size()<<"\n";cout<<"N_COMBO_STD_INTER "<<ncombstd<<" N_COMBO_ALL_INTER "<<ncomball<<" N_COMBO_7_INTER "<<ncombseven<<"\n";cout<<"N_TOTAL "<<(nstd+n7-n7std+n13+nall+ncomb-ncombstd-ncomball-ncombseven)<<"\n";
}
