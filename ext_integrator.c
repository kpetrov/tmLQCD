/* $Id$ */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "su3.h"
#include "su3adj.h"
#include "expo.h"
#include "ranlxd.h"
#include "sse.h"
#include "global.h"
#include "linalg_eo.h"
#include "clover_eo.h"
#include "start.h"
#include "sw.h"
#include "linsolve.h"
#include "xchange.h"
#include "deriv_Sb.h"
#include "Hopping_Matrix.h"
#include "tm_operators.h"
#include "hybrid_update.h"
#include "derivative_psf.h"
#include "ext_integrator.h"

extern int count00, count01, count10, count11, count20, count21;

void ext_integrator(int * const n_int, const double tau, const int S) {
  int i,j;
  double eps0, eps;

  if(S == g_nr_of_psf) {
    /* initialize the counter for the inverter */
    count00=0; count01=0; count10=0; count11=0; count20=0; count21=0;
  }

  eps = tau/((double)n_int[S]);

  if(S == 1) {
    eps0 = eps/((double)n_int[0]);
    update_fermion_momenta(0.5 * eps, S-1);
    gauge_momenta(0.5 * eps0);
    for(j = 0; j < n_int[S]; j++) {
      for(i = 0; i < n_int[0]; i++) {
	update_gauge(eps0); 
	gauge_momenta(eps0);
      }
      update_fermion_momenta(eps, S-1);
    }
    for(i = 1; i < n_int[0]; i++) {
      update_gauge(eps0); 
      gauge_momenta(eps0);
    }
    update_gauge(eps0); 
    gauge_momenta(0.5 * eps0);
    update_fermion_momenta(0.5 * eps, S-1);
  }
  else {
    update_fermion_momenta(0.5 * eps, S-1);
    for(i = 1; i < n_int[S]; i++){
      ext_integrator(n_int, eps, S-1);
      update_fermion_momenta(eps, S-1);
    }
    ext_integrator(n_int, eps, S-1);
    update_fermion_momenta(0.5 * eps, S-1);
  }
}
