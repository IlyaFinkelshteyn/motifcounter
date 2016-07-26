#include <R.h>
#include "sequence.h"
#include "overlap.h"
#include "background.h"

double *Rstation=NULL, *Rtrans=NULL;
int Rorder;
void RdestroyBackground();

void Rmakebg(char **infasta, int *order, int *nseq, int *lseq) {
  FILE *f;
  double *count;

  RdestroyBackground();
  f =fopen(infasta[0],"r");
  if (f==NULL) {
    error("%s not found!",infasta[0]);
    return;
  }

  if (order[0]>0) {
    Rstation=Calloc(power(ALPHABETSIZE,order[0]),double);
    count=Calloc(power(ALPHABETSIZE,order[0]+1),double);
    Rtrans=Calloc(power(ALPHABETSIZE, order[0]+1),double);
    if (Rstation==NULL || count==NULL || Rtrans==NULL) {
    	error("Memory allocation in Rmakebg failed");
		}

    getNucleotideFrequencyFromSequence(f,count, order[0], nseq, lseq);

    getForwardTransition(count, Rtrans, order[0]);
    getStationaryDistribution(Rtrans, Rstation, order[0]);

  } else {
    Rstation=Calloc(power(ALPHABETSIZE,order[0]+1),double);
    Rtrans=Calloc(power(ALPHABETSIZE,order[0]+1),double);
    count=Calloc(power(ALPHABETSIZE,order[0]+1),double);
    if (Rstation==NULL || count==NULL || Rtrans==NULL) {
    	error("Memory allocation in Rmakebg failed");
		}
    getNucleotideFrequencyFromSequence(f,count, order[0], nseq,lseq);
    getForwardTransition(count, Rstation, order[0]);
    getForwardTransition(count, Rtrans, order[0]);
  }
  fclose(f);
  Rorder=order[0];

  Free(count);
}

void RgetOrder(int *o) {
  o[0]=Rorder;
}
void RgetBackground(double *station, double *trans) {
  int i;
	if (Rstation==NULL) return;
  for (i=0; i<4; i++) {
    Rprintf("%e\n", Rstation[i]);
  }
  for (i=0; i<4; i++) {
    Rprintf("%e\n", Rtrans[i]);
  }
}

void RprintBackground() {
	printBackground(Rstation,Rtrans,Rorder);
}

void RdestroyBackground() {

  if(Rstation) Free(Rstation);
  if(Rtrans) Free(Rtrans);
  Rstation=NULL;
  Rtrans=NULL;
}

