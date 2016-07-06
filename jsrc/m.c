/* Copyright 1990-2010, Jsoftware Inc.  All rights reserved.               */
/* Licensed use only. Any other use is in violation of copyright.          */
/*                                                                         */
/* Memory Management                                                       */

#ifdef _WIN32
#include <windows.h>
#else
#define __cdecl
#endif

#include "j.h"

#define MEMAUDIT 0   // Audit level: 0=fastest, 1=buffer checks but not tstack 2=buffer+tstack

#define PSIZE       65536L         /* size of each pool                    */
#define PLIM        1024L          /* pool allocation for blocks <= PLIM   */
#define PLIML       10L            /* base 2 log of PLIM                   */

// Don't call traverse unless one of these bits is set
#define TRAVERSIBLE (XD|RAT|XNUM|BOX|VERB|ADV|CONJ|SB01|SINT|SFL|SCMPX|SLIT|SBOX)
#define TOOMANYATOMS 0x01000000000000LL  // more atoms than this is considered overflow

static void jttraverse(J,A,AF);

// msize[k]=2^k, for sizes up to the size of an I.  Not used in this file any more
B jtmeminit(J jt){I k,m=MLEN;
 k=1; DO(m, msize[i]=k; k+=k;);  /* OK to do this line in each thread */
 jt->tbase=-NTSTACK;
 jt->ttop = NTSTACK;
 jt->mmax =msize[m-1];
 DO(m, jt->mfree[i]=0; jt->mfreeb[i]=0; jt->mfreet[i]=1048576;);
 R 1;
}


F1(jtspcount){A z;I c=0,j,m=1+PLIML,*v;MS*x;
 ASSERTMTV(w);
 GA(z,INT,2*m,2,0); v=AV(z);
 DO(m, j=0; x=(MS*)(jt->mfree[i]); while(x){x=(MS*)(x->a); ++j;} if(j){++c; *v++=msize[i]; *v++=j;});
 v=AS(z); v[0]=c; v[1]=2; AN(z)=2*c;
 R z;
}    /* 7!:3 count of unused blocks */

static int __cdecl spfreecomp(const void *x,const void *y){R *(I*)x<*(I*)y?-1:1;}

B jtspfree(J jt){A t;I c,d,i,j,m,n,*u,*v;MS*x;
 m=0; u=5+jt->mfreet; v=5+jt->mfreeb;
 /* DO(1+PLIML, if(jt->mfreet[i]<=jt->mfreeb[i]){j=jt->mfreeb[i]/msize[i]; m=MAX(m,j);}); */
 if(*++u<=*++v){j=*v/  64; m=MAX(m,j);}
 if(*++u<=*++v){j=*v/ 128; m=MAX(m,j);}
 if(*++u<=*++v){j=*v/ 256; m=MAX(m,j);}
 if(*++u<=*++v){j=*v/ 512; m=MAX(m,j);}
 if(*++u<=*++v){j=*v/1024; m=MAX(m,j);}
 if(!m)R 1;
 GA(t,INT,1+m,1,0); v=AV(t);
 /* must not allocate memory after this point */
 for(i=6;i<=PLIML;++i){
  if(jt->mfreet[i]>jt->mfreeb[i])continue;
  n=0; x=(MS*)(jt->mfree[i]); 
  while(x){v[n++]=(I)x; x=(MS*)(x->a);}
  qsort(v,n,SZI,spfreecomp); 
  j=0; u=0; c=msize[i]; d=PSIZE/c;
  while(n>j){
   x=(MS*)v[j];
   if(MFHEAD&x->mflag&&n>=j+d&&PSIZE==c+v[j+d-1]-v[j]){
    j+=d; 
    FREE(x); 
    jt->mfreeb[i]-=PSIZE;
   }else{x->a=u; u=(I*)v[j]; ++j;}
  }
  jt->mfree[i]=u; jt->mfreet[i]=1048576+jt->mfreeb[i];
 }
 R 1;
}    /* free unused blocks */

static F1(jtspfor1){
 RZ(w);
 if(BOX&AT(w)){A*wv=AAV(w);I wd=(I)w*ARELATIVE(w); DO(AN(w), spfor1(WVR(i)););}
 else if(AT(w)&TRAVERSIBLE)traverse(w,jtspfor1); 
 if(1e9>AC(w)||AFSMM&AFLAG(w))
  if(AFNJA&AFLAG(w)){I j,m,n,p;
   m=SZI*WP(AT(w),AN(w),AR(w)); 
   n=p=m+mhb; 
   j=6; n>>=j; 
   while(n){n>>=1; ++j;} 
   if(p==msize[j-1])--j;
   jt->spfor+=msize[j];
  }else jt->spfor+=msize[((MS*)w-1)->j];
 R mtm;
}

F1(jtspfor){A*wv,x,y,z;C*s;D*v,*zv;I i,m,n,wd;
 RZ(w);
 n=AN(w); wv=AAV(w); wd=(I)w*ARELATIVE(w); v=&jt->spfor;
 ASSERT(!n||BOX&AT(w),EVDOMAIN);
 GA(z,FL,n,AR(w),AS(w)); zv=DAV(z); 
 for(i=0;i<n;++i){
  x=WVR(i); m=AN(x); s=CAV(x);
  ASSERT(LIT&AT(x),EVDOMAIN);
  ASSERT(1>=AR(x),EVRANK);
  ASSERT(vnm(m,s),EVILNAME);
  RZ(y=symbrd(nfs(m,s))); 
  *v=0.0; spfor1(y); zv[i]=*v;
 }
 R z;
}    /* 7!:5 space for named object; w is <'name' */

F1(jtspforloc){A*wv,x,y,z;C*s;D*v,*zv;I c,i,j,m,n,wd,*yv;L*u;
 RZ(w);
 n=AN(w); wv=AAV(w); wd=(I)w*ARELATIVE(w); v=&jt->spfor;
 ASSERT(!n||BOX&AT(w),EVDOMAIN);
 GA(z,FL,n,AR(w),AS(w)); zv=DAV(z); 
 for(i=0;i<n;++i){
  x=WVR(i); m=AN(x); s=CAV(x);
  if(!m){m=4; s="base";}
  ASSERT(LIT&AT(x),EVDOMAIN);
  ASSERT(1>=AR(x),EVRANK);
  ASSERT(vlocnm(m,s),EVILNAME);
  y=stfind(0,m,s);
  ASSERT(y,EVLOCALE);
  *v=(D)msize[((MS*)y-1)->j];
  spfor1(LOCPATH(y)); spfor1(LOCNAME(y));
  m=AN(y); yv=AV(y); 
  for(j=1;j<m;++j){
   c=yv[j];
   while(c){*v+=sizeof(L); u=c+jt->sympv; spfor1(u->name); spfor1(u->val); c=u->next;}
  }
  zv[i]=*v;
 }
 R z;
}    /* 7!:6 space for a locale */


F1(jtmmaxq){ASSERTMTV(w); R sc(jt->mmax);}
     /* 9!:20 space limit query */

F1(jtmmaxs){I j,m=MLEN,n;
 RE(n=i0(vib(w)));
 ASSERT(1E5<=n,EVLIMIT);
 j=m-1; DO(m, if(n<=msize[i]){j=i; break;});
 jt->mmax=msize[j];
 R mtm;
}    /* 9!:21 space limit set */

#if MEMAUDIT>=1
// Verify that block w does not appear on tstack more than lim times
static void audittstack(J jt, A w, I lim){
 // loop through each block of stack
 I base; A* tstack; I ttop,stackct=0;
 for(base=jt->tbase,tstack=jt->tstack,ttop=jt->ttop;base>=0;base-=NTSTACK){I j;
  // loop through each entry, skipping the first which is a chain
  for(j=1;j<ttop;++j)if(tstack[j]==w){
   ++stackct;   // increment number of times we have seen w
   if(stackct>lim)*(I*)0=0;  // if too many, abort
  }
  // back up to previous block
  base -= NTSTACK;  // this leaves a gap but it matches other code
  ttop = NTSTACK;
  if(base>=0)tstack=AAV(tstack[0]); // back up to data for previous field
 }
}
#endif

void jtfr(J jt,A w){I j,n;MS*x;
 if(!w)R;
 x=(MS*)w-1;   // point to free header
#if MEMAUDIT>=1
 if(!(AFLAG(w)&(AFNJA|AFSMM)||x->a==(I*)0xdeadbeef))*(I*)0=0;  // testing - verify block is memmapped/SMM or allocated
#endif
 if(ACDECR(w)>0)R;  // fall through if decr to 0, or from 100001 to 100000
#if MEMAUDIT>=1
 if(ACUC(w))*(I*)0=0;  // usecount should not go below 0
#if MEMAUDIT>=2
 audittstack(jt,w,0);  // must not free anything on the stack
#endif
#endif
 // SYMB must free as a monolith, with the symbols returned when the hashtables are
 if(AT(w)==SYMB) {I j,k,kt,wn=AN(w),*wv=AV(w);
  fr(LOCPATH(w));
  fr(LOCNAME(w));
  for(j=1;j<wn;++j){
   // free the chain; kt->last block freed
   // Use fr for the name, since it has no descendants, but use fa for the value, which may have contents.
   // Tidy up fields for the next user (bad practice, but that's how it was done)
   for(k=wv[j];k;k=(jt->sympv)[k].next){kt=k;fr((jt->sympv)[k].name);fa((jt->sympv)[k].val);(jt->sympv)[k].name=0;(jt->sympv)[k].val=0;(jt->sympv)[k].sn=0;(jt->sympv)[k].flag=0;(jt->sympv)[k].prev=0;}  // prev for 18!:31
   // if the chain is not empty, make it the base of the free pool & chain previous pool from it
   if(k=wv[j]){(jt->sympv)[kt].next=jt->sympv->next;jt->sympv->next=k;}
  }
 }
 j=x->j;
#if MEMAUDIT>=1
 if(j<6||j>63)*(I*)0=0;  // pool number must be valid
#endif
 n=1LL<<j;
 if(PLIML<j)FREE(x);  /* malloc-ed       */
 else{                /* pool allocation */
  x->a=jt->mfree[j]; 
  jt->mfree[j]=(I*)x; 
  jt->mfreeb[j]+=n;
#if MEMAUDIT>=1
  x->j=0xdeaf;
#endif
 }
 jt->bytes-=n;
}

void jtfh(J jt,A w){fr(w);}

static A jtma(J jt,I m){A z;C*u;I j,n,p,*v;MS*x;
 p=m+mhb; 
 ASSERT((UI)p<=(UI)jt->mmax,EVLIMIT);
 for(n=64;n<p;n=n+n);
 j=CTTZI(n);
 JBREAK0;  // Here to allow instruction scheduling
 if(jt->mfree[j]){         /* allocate from free list         */
  z=(A)(mhw+jt->mfree[j]);
#if MEMAUDIT>=1
  if(((MS*)z-1)->j!=(S)0xdeaf)*(I*)0=0;  // verify block has free-pool marker
#endif
  jt->mfree[j]=((MS*)(jt->mfree[j]))->a;
  jt->mfreeb[j]-=n;
 }else if(j>PLIML){         /* large block: straight malloc    */
  v=MALLOC(n);
  ASSERT(v,EVWSFULL); 
  z=(A)(v+mhw);
 }else{                    /* small block: do pool allocation */
  v=MALLOC(PSIZE);
  ASSERT(v,EVWSFULL);
  u=(C*)v; DO(PSIZE>>j, x=(MS*)u; u+=n; x->a=(I*)u; x->mflag=0;); x->a=0;  // chain blocks to each other; set chain of last block to 0
#if MEMAUDIT>=1
   u=(C*)v; DO(PSIZE>>j, ((MS*)u)->j=0xdeaf; u+=n;);
#endif
  ((MS*)v)->mflag=MFHEAD;
  z=(A)(mhw+v); 
  jt->mfree[j]=((MS*)v)->a;
  jt->mfreeb[j]+=PSIZE-n;
 }
 if(jt->bytesmax<(jt->bytes+=n))jt->bytesmax=jt->bytes;
 x=(MS*)z-1; x->j=(C)j;  // Why clear a?
#if MEMAUDIT>=1
 x->a=(I*)0xdeadbeef;  // flag block as allocated
#endif
 R z;
}

static void jttraverse(J jt,A w,AF f){
// obsolete RZ(w);
 switch(CTTZ(AT(w))){
  case XDX:
   {DX*v=(DX*)AV(w); DO(AN(w), CALL1(f,v->x,0L); ++v;);} break;
  case RATX:  
   {A*v=AAV(w); DO(2*AN(w), CALL1(f,*v++,0L););} break;
  case XNUMX: case BOXX:
   if(!(AFLAG(w)&AFNJA+AFSMM)){A*wv=AAV(w);I wd=(I)w*ARELATIVE(w); DO(AN(w), CALL1(f,WVR(i),0L););} break;
  case VERBX: case ADVX:  case CONJX: 
   {V*v=VAV(w); CALL1(f,v->f,0L); CALL1(f,v->g,0L); CALL1(f,v->h,0L);} break;
  case SB01X: case SINTX: case SFLX: case SCMPXX: case SLITX: case SBOXX:
   {P*v=PAV(w); CALL1(f,SPA(v,a),0L); CALL1(f,SPA(v,e),0L); CALL1(f,SPA(v,i),0L); CALL1(f,SPA(v,x),0L);} break;
 }
// obsolete R mark;
}

static A jttg(J jt){A t=jt->tstacka,z;
 RZ(z=ma(SZI*WP(BOX,NTSTACK,1L)));
 AT(z)=BOX; AC(z)=ACUC1; AR(z)=1; AN(z)=*AS(z)=NTSTACK; AM(z)=NTSTACK*SZA; AK(z)=AKX(z);
 jt->tstacka=z; jt->tstack=AAV(jt->tstacka); jt->tbase+=NTSTACK; jt->ttop=1;
 *jt->tstack=t;
 R z;
}

static void jttf(J jt){A t=jt->tstacka;
 jt->tstacka=*jt->tstack; jt->tstack=AAV(jt->tstacka); jt->tbase-=NTSTACK; jt->ttop=NTSTACK;
 fr(t);
}

F1(jttpush){
 RZ(w);
 if(AT(w)&TRAVERSIBLE)traverse(w,jttpush);
 if(jt->ttop>=NTSTACK)RZ(tg());
 jt->tstack[jt->ttop]=w;
 ++jt->ttop;
#if MEMAUDIT>=2
 audittstack(jt,w,ACUC(w));  // verify total # w on stack does not exceed usecount
#endif
 R w;
}

I jttpop(J jt,I old){
 while(old<jt->tbase+jt->ttop)if(1<jt->ttop)fr(jt->tstack[--jt->ttop]); else tf(); 
 R old;
}

A jtgc (J jt,A w,I old){ra(w); tpop(old); R tpush(w);}

void jtgc3(J jt,A x,A y,A z,I old){
 if(x)ra(x);    if(y)ra(y);    if(z)ra(z);
 tpop(old);
 if(x)tpush(x); if(y)tpush(y); if(z)tpush(z);
}


F1(jtfa ){RZ(w); if(AT(w)&TRAVERSIBLE)traverse(w,jtfa); fr(w);   R mark;}
F1(jtra ){RZ(w); if(AT(w)&TRAVERSIBLE)traverse(w,jtra); ACINCR(w); R w;   }

static F1(jtra1){RZ(w); if(AT(w)&TRAVERSIBLE)traverse(w,jtra1); ACINCRBY(w,jt->arg); R w;}
A jtraa(J jt,I k,A w){A z;I m=jt->arg; jt->arg=k; z=ra1(w); jt->arg=m; R z;}

F1(jtrat){R tpush(ra(w));}

A jtga(J jt,I t,I n,I r,I*s){A z;I m,w;
 if(t&BIT){const I c=8*SZI;              /* bit type: pad last axis to fullword */
  ASSERTSYS(1>=r||s,"ga bit array shape");
  if(1>=r)w=(n+c-1)/c; else RE(w=mult(prod(r-1,s),(s[r-1]+c-1)/c));
#if SY_64
  m=BP(INT,0L,r);
  ASSERT((UI)n<TOOMANYATOMS,EVLIMIT);
#else
  // This test is not perfect but it's close
  w+=WP(INT,0L,r); m=SZI*w; 
  ASSERT(     n>=0&&m>w&&w>0,EVLIMIT);   /* beware integer overflow */
#endif
 }else{
#if SY_64
  m=BP(t,n,r);
  ASSERT((UI)n<TOOMANYATOMS,EVLIMIT);
#else
  w=WP(t,n,r);     m=SZI*w; 
  ASSERT(m>n&&n>=0&&m>w&&w>0,EVLIMIT);   /* beware integer overflow */
#endif
 }
 RZ(z=ma(m));
#if MEMAUDIT>=2
 audittstack(jt,z,0);  // verify buffer not on stack
#endif
 if(!(t&DIRECT))memset(z,C0,m);
 if(t&LAST0){I*v=(I*)((C*)z+m); v[-1]=0; v[-2]=0;}  // if LAST0, clear the last two Is.
 AC(z)=ACUC1; AN(z)=n; AR(z)=r; AFLAG(z)=0; AK(z)=AKX(z); AM(z)=msize[((MS*)z-1)->j]-(AK(z)+sizeof(MS)); 
 AT(z)=0; tpush(z); AT(z)=t;
 if(1==r&&!(t&SPARSE))*AS(z)=n; else if(r&&s)ICPY(AS(z),s,r);  /* 1==n always if t&SPARSE */
 R z;
}

A jtgah(J jt,I r,A w){A z;
 ASSERT(RMAX>=r,EVLIMIT); 
 RZ(z=ma(SZI*(AH+r)));
 AT(z)=0; AC(z)=ACUC1; tpush(z);  // original had ++AC(z)!?
 if(w){
  AFLAG(z)=0; AM(z)=AM(w); AT(z)=AT(w); AN(z)=AN(w); AR(z)=r; AK(z)=CAV(w)-(C*)z;
  if(1==r)*AS(z)=AN(w);
 }
 R z;
}    /* allocate header */ 

// clone w, returning the address of the cloned area
F1(jtca){A z;I t;P*wp,*zp;
 RZ(w);
 t=AT(w);
 GA(z,t,AN(w),AR(w),AS(w)); if(AFLAG(w)&AFNJA+AFSMM+AFREL)AFLAG(z)=AFREL;
 if(t&SPARSE){
  wp=PAV(w); zp=PAV(z);
  SPB(zp,a,ca(SPA(wp,a)));
  SPB(zp,e,ca(SPA(wp,e)));
  SPB(zp,i,ca(SPA(wp,i)));
  SPB(zp,x,ca(SPA(wp,x)));
 }else MC(AV(z),AV(w),AN(w)*bp(t)+(t&NAME?sizeof(NM):0)); 
 R z;
}

F1(jtcar){A*u,*wv,z;I n,wd;P*p;V*v;
 RZ(z=ca(w));
 n=AN(w);
 switch(AT(w)){
  case RAT:  n+=n;
  case XNUM:
  case BOX:  u=AAV(z); wv=AAV(w); wd=(I)w*ARELATIVE(w); DO(n, RZ(*u++=car(WVR(i)));); break;
  case SB01: case SLIT: case SINT: case SFL: case SCMPX: case SBOX:
   p=PAV(z); 
   SPB(p,a,car(SPA(p,a)));
   SPB(p,e,car(SPA(p,e)));
   SPB(p,i,car(SPA(p,i)));
   SPB(p,x,car(SPA(p,x)));
   break;
  case VERB: case ADV: case CONJ: 
   v=VAV(z); 
   if(v->f)RZ(v->f=car(v->f)); 
   if(v->g)RZ(v->g=car(v->g)); 
   if(v->h)RZ(v->h=car(v->h));
 }
 R z;
}

B jtspc(J jt){A z; RZ(z=MALLOC(1000)); FREE(z); R 1; }

A jtext(J jt,B b,A w){A z;I c,k,m,m1,t;
 RZ(w);                               /* assume AR(w)&&AN(w)    */
 m=*AS(w); c=AN(w)/m; t=AT(w); k=c*bp(t);
 GA(z,t,2*AN(w),AR(w),AS(w)); 
 MC(AV(z),AV(w),m*k);                 /* copy old contents      */
 if(b){ra(z); fa(w);}                 /* 1=b iff w is permanent */
 *AS(z)=m1=AM(z)/k; AN(z)=m1*c;       /* "optimal" use of space */
 if(!(t&DIRECT))memset(CAV(z)+m*k,C0,k*(m1-m));
 R z;
}

A jtexta(J jt,I t,I r,I c,I m){A z;I k,m1; 
 GA(z,t,m*c,r,0); 
 k=bp(t); *AS(z)=m1=AM(z)/(c*k); AN(z)=m1*c;
 if(2==r)*(1+AS(z))=c;
 if(!(t&DIRECT))memset(AV(z),C0,k*AN(z));
 R z;
}    /* "optimal" allocation for type t rank r, c atoms per item, >=m items */


/* debugging tools  */

B jtcheckmf(J jt){C c;I i,j;MS*x,*y;
 for(j=0;j<=PLIML;++j){
  i=0; y=0; x=(MS*)(jt->mfree[j]); /* head ptr for j-th pool */
  while(x){
   ++i; c=x->mflag;
   if(!(j==x->j)){
    ASSERTSYS(0,"checkmf 0");
   }
   if(!(!c||c==MFHEAD)){
    ASSERTSYS(0,"checkmf 1");
   }
   y=x; x=(MS*)x->a;
 }}
 R 1;
}    /* traverse free list */

B jtchecksi(J jt){DC d;I dt;
 d=jt->sitop;
 while(d&&!(DCCALL==d->dctype&&d->dcj)){
  dt=d->dctype;
  if(!(dt==DCPARSE||dt==DCSCRIPT||dt==DCCALL||dt==DCJUNK)){
   ASSERTSYS(0,"checksi 0");
  }
  if(!(d!=d->dclnk)){
   ASSERTSYS(0,"checksi 1");
  }
  d=d->dclnk;
 }
 R 1;
}    /* traverse stack per jt->sitop */
