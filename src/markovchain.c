#include <stdlib.h>
#include <stdio.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <math.h>
#include <string.h>
#ifdef IN_R
#include <R.h>
#include <Rinternals.h>
#include <Rmath.h>
#endif

#include "background.h"
#include "forground.h"
#include "matrix.h"
#include "score2d.h"
#include "overlap.h"
//#include "countdist.h"
#include "combinatorial.h"
#include "markovchain.h"

extern double *Rstation, *Rtrans;

double OverlapHit(int N, double *beta, double *betap) {
    int i;
    double d=1.0, n=1.0;

    //if (N<0 || N>=Rpwm->nrow) error("wrong index, i=%d\n", N);

    // beta ... forward hit
    // betap .. reverse hit either 3p or 5p
    // compute denuminator
    for (i=0; i<N; i++) {
        d-=(beta[i]+betap[i]);
    }
    n=(beta[N]);
    if (d<=0.0) return 0.0;

    return (n/d);
}

double NoOverlapHit(int N, double *beta, double *betap) {
    int i;
    double d=1.0, n=1.0;

    //if (N<0) error("wrong index, i=%d\n", N);

    // beta ... forward hit
    // betap .. reverse hit either 3p or 5p
    // compute denuminator
    for (i=0; i<N; i++) {
        d-=(beta[i]+betap[i]);
    }
    n=d-(beta[N]+betap[N]);
    if (d<=0.0) return 0.0;

    return (n/d);
}

#undef DEBUG
#define DEBUG
static double *Rdist=NULL;
void markovchain(double *dist, double *a,
 double *beta, double *beta3p, double *beta5p, int slen, int motiflen) {
    int i, k;
    double *post, *prior;
    double alphacond;

    if (!Rstation||!Rtrans) {
        error("load forground and background properly");
        return;
    }

    // the states are
    // dist[0] ... p(nohit)
    // dist[1] ... p(Hf)
    // dist[2] ... p(Hr)
    // dist[3 ... 3+M-1] ... p(n0), ... , p(nL)
    // dist[3+M, ..., 3+M+M-2] ... p(n1'), ..., p(nL')
    //
    post=Calloc(2*motiflen+2, double);
    if(post==NULL) {
        error("Memory-allocation in markovchain failed");
    }
    prior=dist;
    alphacond=a[0];
    memset(prior, 0, (2*motiflen+2)*sizeof(double));
    prior[0]=1.;

    for (k=0; k<slen; k++) {
        // P(N)
        post[0]=(1-alphacond*(2-beta3p[0]))*(prior[0]+prior[motiflen+2] + 
        prior[2*motiflen+1]);

        // P(Hf)
        post[1]=alphacond*(prior[0]+prior[motiflen+2] +
            prior[2*motiflen+1]);

        for (i=1;i<motiflen;i++) {
            post[1]+=OverlapHit(i, beta, beta3p)*prior[3+i-1];
        }
        for (i=2;i<motiflen;i++) {
            post[1]+=OverlapHit(i, beta5p, beta)*prior[motiflen+3+i-2];
        }
        post[1]+=beta5p[1]*prior[2];


        // P(Hr)
        post[2]=alphacond*(1-beta3p[0])*(prior[0]+prior[motiflen+2] +
            prior[2*motiflen+1]);
        for (i=2;i<motiflen;i++) {
            post[2]+=OverlapHit(i, beta, beta5p)*prior[motiflen+3+i-2];
        }
        for (i=1;i<motiflen;i++) {
            post[2]+=OverlapHit(i, beta3p, beta)*prior[3+i-1];
        }
        // should i switch this line
        post[2]+=beta3p[0]*prior[1];
        post[2]+=beta[1]*prior[2];

        // P(n0)
        post[3]=NoOverlapHit(0, beta, beta3p)*prior[1];
        for (i=1;i<motiflen;i++) {
            post[3+i]=NoOverlapHit(i, beta,beta3p)*prior[3+i-1];
        }
        // P(n1')
        post[3+motiflen]=NoOverlapHit(1, beta, beta5p)*prior[2];
        for (i=2;i<motiflen;i++) {
            post[motiflen+3+i-1]=NoOverlapHit(i, beta,beta5p)*
                prior[motiflen+3+i-2];
        }
        memcpy(prior, post, (2*motiflen+2)*sizeof(double));
        memset(post, 0, (2*motiflen+2)*sizeof(double));
    }

    Free(post);
}

void dmc(int n, double *alphacond, double *gradient, void *ex) {

    double val;
    CGParams *cgparams=(CGParams*)ex;
    double epsilon;
    double pa,ma;

    if (!Rstation||!Rtrans) {
        error("load forground and background properly");
        return;
    }

    if (!Rdist) {
        Rdist=Calloc(2*cgparams->motiflen+2, double);
        if(Rdist==NULL) {
            error("Memory-allocation in dmc failed");
        }
    }

    epsilon=alphacond[0]/1000;
    pa=*alphacond + epsilon;
    ma=*alphacond - epsilon;
    markovchain(Rdist, &pa, cgparams->beta, 
            cgparams->beta3p, cgparams->beta5p, 
            cgparams->len,cgparams->motiflen);

    val=Rdist[1]+Rdist[2];
    markovchain(Rdist, &ma, cgparams->beta, 
            cgparams->beta3p, cgparams->beta5p, 
            cgparams->len,cgparams->motiflen);

    val-=(Rdist[1]+Rdist[2]);
    val/=2*epsilon;

    markovchain(Rdist, alphacond, cgparams->beta, 
            cgparams->beta3p, cgparams->beta5p, 
            cgparams->len,cgparams->motiflen);

    *gradient=-2*(2*cgparams->alpha-Rdist[1]-Rdist[2])*val;
}
double minmc(int n, double *alpha, void *ex) {

    //double *extra=(double*)ex;
    CGParams *cgparams=(CGParams*)ex;

    if (!Rdist) {
        Rdist=Calloc(2*cgparams->motiflen+2, double);
        if(Rdist==NULL) {
            error("Memory-allocation in minmc failed");
        }
    }

    markovchain(Rdist, alpha, cgparams->beta, 
            cgparams->beta3p, cgparams->beta5p, 
            cgparams->len,cgparams->motiflen);

    return R_pow_di(2*cgparams->alpha-Rdist[1]-Rdist[2], 2);
}

#ifdef OBSOLETE_
SEXP getMarkovProb(SEXP niter, 
		SEXP alpha, SEXP beta, SEXP beta3p, SEXP beta5p) {
    int i, n;
    int _niter=500;
    double _alpha;
    double *_beta,*_beta3p,*_beta5p;
    double *_rm;
    int nrow;
    SEXP retMatrix;

    _niter=INTEGER(niter)[0];
    _alpha=REAL(alpha)[0];
    beta = PROTECT(coerceVector(beta,REALSXP));
    beta3p = PROTECT(coerceVector(beta3p,REALSXP));
    beta5p = PROTECT(coerceVector(beta5p,REALSXP));

    _beta=REAL(beta);
    _beta3p=REAL(beta3p);
    _beta5p=REAL(beta5p);

    nrow=2*motiflen+2;
    PROTECT(retMatrix=allocMatrix(REALSXP,nrow, _niter));

    _rm=REAL(retMatrix);

    if (!Rdist) {
        Rdist=Calloc(nrow, double);
        if(Rdist==NULL) {
            error("Memory-allocation in minmc failed");
        }
    }
    for (i=0;i<_niter;i++) {
        markovchain(Rdist, &_alpha, _beta, _beta3p, _beta5p,i);
        for (n=0;n<nrow;n++) {
            _rm[i*nrow+n]=Rdist[n];
        }
    }
    UNPROTECT(4);
    removeDist();
    return retMatrix;
}
#endif

void removeDist() {
    if(Rdist) Free(Rdist);
    Rdist=NULL;
}

#ifdef OBSOLETE_
// sampling from the Markov model for debugging purposes:
void sampling_markovchain(double *a,
 double *beta, double *beta3p, double *beta5p, int slen, int nos, int nperm) {
    int i;
    int *cnt, perm,seq,pos;
    int state=0;
    double mcnt, vcnt, val, pa;

    cnt=Calloc(nperm, int);
    if(cnt==NULL) {
        error("Memory-allocation in sampling_markovchain failed");
    }

    if (!Rstation||!Rtrans) {
        error("load forground and background properly");
        return;
    }
    // state=0 is no hit
    // state=1 is forward strand hit
    // state=2 is reverse strand hit
    GetRNGstate();

    for (perm=0; perm<nperm; perm++) {
        cnt[perm]=0;
        for (seq=0;seq<nos; seq++) {
            for (pos=0; pos<slen; ) {
                val=unif_rand();
                if (state==0) {
                    if (val<=1-a[0]*(2-beta3p[0])) {
                        // state remains the same
                        pos++;
                    } else if (1-a[0]*(2-beta3p[0])< val && val <= 1-a[0]) {
                            state=2;
                    } else {
                            state=1;
                    }
                } else if (state==1) {
                    cnt[perm]++;
                    pa=0.0;
                    for (i=0; i<motiflen && pa<val; i++) {
                        pa+=beta[i];
                        if (val<=pa) {
                            pos+=i;
                            state=1;
                            break;
                        }
                    }
                    for (i=0; i<motiflen && pa<val; i++) {
                            pa+=beta3p[i];
                        if (val<=pa) {
                            pos+=i;
                            state=2;
                            break;
                        }
                    }
                    if (pa<val) {
                        pos+=motiflen;
                        state=0;
                    }
                } else if (state==2) {
                    cnt[perm]++;
                    pa=0.0;
                    for (i=0; i<motiflen && pa<val; i++) {
                        pa+=beta[i];
                        if (val<=pa) {
                            pos+=i;
                            state=2;
                            break;
                        }
                    }
                    for (i=0; i<motiflen && pa<val; i++) {
                        pa+=beta5p[i];
                        if (val<=pa) {
                            pos+=i;
                            state=1;
                            break;
                        }
                    }
                    if(pa<val) {
                        pos+=motiflen;
                        state=0;
                    }
                }
            }
        }
    }
    mcnt=0.0;
    for (i=0; i<nperm; i++) {
        mcnt+=(double)cnt[i];
    }
    mcnt/=(double)nperm;
    vcnt=0.0;
    for (i=0; i<nperm; i++) {
        vcnt+=(double) ((double)cnt[i]-mcnt)*
            ((double)cnt[i]-mcnt)/((double)(nperm-1));
    }
    // the states are
    // dist[0] ... p(nohit)
    // dist[1] ... p(Hf)
    // dist[2] ... p(Hr)
    // dist[3 ... 3+M-1] ... p(n0), ... , p(nL)
    // dist[3+M, ..., 3+M+M-2] ... p(n1'), ..., p(nL')
    //
    //Rprintf("mean(count)=%e\n", mcnt);
    //Rprintf("var(count)=%e\n",vcnt);

    PutRNGstate();
}

SEXP sample_mc( SEXP alpha, SEXP beta, SEXP beta3p, 
        SEXP beta5p, SEXP seqlen, SEXP nos, SEXP nperm) {

    double _alpha;
    double *_beta,*_beta3p,*_beta5p;
    int _slen,_nos,_nperm;

    _slen=INTEGER(seqlen)[0];
    _nos=INTEGER(nos)[0];
    _nperm=INTEGER(nperm)[0];
    _alpha=REAL(alpha)[0];
    beta = PROTECT(coerceVector(beta,REALSXP));
    beta3p = PROTECT(coerceVector(beta3p,REALSXP));
    beta5p = PROTECT(coerceVector(beta5p,REALSXP));

    _beta=REAL(beta);
    _beta3p=REAL(beta3p);
    _beta5p=REAL(beta5p);

    sampling_markovchain(&_alpha, _beta, _beta3p, _beta5p, _slen, _nos, _nperm);

    UNPROTECT(3);
    return R_NilValue;
}
#endif
