#include <R.h>
#include "sequence.h"
#include "overlap.h"
#include "background.h"

double *RstationForSampling=NULL, *RtransForSampling=NULL;
int RorderForSampling;
void RdestroyBackgroundForSampling();

void RmakebgForSampling(char **infasta, int *order, int *nseq, int *lseq) {
  FILE *f;
  double *count;

  RdestroyBackgroundForSampling();
  f =fopen(infasta[0],"r");
  if (f==NULL) {
    error("%s not found!",infasta[0]);
    return;
  }


  if (order[0]>0) {
    RstationForSampling=Calloc(power(ALPHABETSIZE,order[0]),double);
    count=Calloc(power(ALPHABETSIZE,order[0]+1),double);
    RtransForSampling=Calloc(power(ALPHABETSIZE, order[0]+1),double);
    if (RstationForSampling==NULL || count==NULL || 
    		RtransForSampling==NULL) {
    	error("Memory allocation in RmakebgForSampling failed");
		}

    getNucleotideFrequencyFromSequence(f,count, order[0], nseq, lseq);
    getForwardTransition(count, RtransForSampling, order[0]);
    getStationaryDistribution(RtransForSampling, RstationForSampling, order[0]);

  } else {
    RstationForSampling=Calloc(power(ALPHABETSIZE,order[0]+1),double);
    RtransForSampling=Calloc(power(ALPHABETSIZE,order[0]+1),double);
    count=Calloc(power(ALPHABETSIZE,order[0]+1),double);
    if (RstationForSampling==NULL || count==NULL || 
    		RtransForSampling==NULL) {
    	error("Memory allocation in RmakebgForSampling failed");
		}
    getNucleotideFrequencyFromSequence(f,count, order[0], nseq, lseq);
    getForwardTransition(count, RstationForSampling, order[0]);
    getForwardTransition(count, RtransForSampling, order[0]);
  }
  fclose(f);
  RorderForSampling=order[0];

  Free(count);
}

void RgetOrderForSampling(int *o) {
  o[0]=RorderForSampling;
}
void RgetBackgroundForSampling(double *station, double *trans) {
  int i;
  for (i=0; i<4; i++) {
    Rprintf("%e\n", RstationForSampling[i]);
  }
  for (i=0; i<4; i++) {
    Rprintf("%e\n", RtransForSampling[i]);
  }
}

void RprintBackgroundForSampling() {
  int i;
  int order_;

  if (RorderForSampling>0) order_=RorderForSampling;
  else order_=1;
  for (i=0; i<power(ALPHABETSIZE, order_); i++) {
    Rprintf("mu(i=%d)=%e\n", i, RstationForSampling[i]);
  }
  for (i=0; i<power(ALPHABETSIZE, order_+1); i++) {
    Rprintf("T(i=%d)=%e\n", i, RtransForSampling[i]);
  }
}

void RdestroyBackgroundForSampling() {

  if(RstationForSampling) Free(RstationForSampling);
  if(RtransForSampling) Free(RtransForSampling);
  RstationForSampling=NULL;
  RtransForSampling=NULL;
}

