/* $F PHd: update_tm_nd.c,v 1.39 2006/02/14 16:50:32 urbach Exp $ */

/***********************************************************
 *
 * This routine contains the update part for
 * the PHMC with up to three pseudo fermion fields
 * for twisted mass QCD
 *
 * Author: Thomas Chiarappa <Thomas.Chiarappa@mib.infn.it>
 *
 ***********************************************************/

#ifdef HAVE_CONFIG_H
# include<config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef MPI
# include <mpi.h>
#endif
#include "global.h"
#include "start.h"
#include "sighandler.h"
#include "tm_operators.h"
#include "linalg_eo.h"
#include "io.h"
#include "observables.h"
#include "hybrid_update.h"

/* IF PHMC */
#include "hybrid_nondegenerate_update.h"
#include "Nondegenerate_Matrix.h"
#include "chebyshev_polynomial_nd.h"
#include "Ptilde_nd.h"
#include "reweighting_factor_nd.h"

#include "ranlxd.h"
/* #include "read_input.h" */
#include "linsolve.h"
#include "expo.h"
#include "xchange.h"
#include "measure_rectangles.h"
#include "init_gauge_tmp.h"
#include "ext_integrator.h"
#include "2mn_integrator.h"
#include "solver/chrono_guess.h"
#include "update_backward_gauge.h"
#include "update_tm_nd.h"



int update_tm_nd(const int integtyp, double *plaquette_energy, double *rectangle_energy, 
	      char * filename, const double dtau, const int Nsteps, const int nsmall,
	      const double tau, int * n_int, const int return_check,
	      double * lambda) {
  su3 *v, *w;
  static int ini_g_tmp = 0;
  int rlxd_state[105];
  int ix, mu, accept, i=0, halfstep = 0;
  int saveiter_max = ITER_MAX_BCG;

  /* IF PHMC */
  int j=0, k, ij;
  double temp, sgn, fact, Diff;
  double Ener[8];
  double factor[8];

  complex temp2;

  double yy[1];
  double dh, expmdh, ret_dh=0., ret_gauge_diff=0.;
  double atime=0., etime=0.;
  int idis0=0, idis1=0, idis2=0;
  int ret_idis0=0, ret_idis1=0, ret_idis2=0;
/*   double lambda[5] = {0.1931833275037836,0.1931833275037836,0.1931833275037836,0.1931833275037836,0.1931833275037836}; */
/*   double lambda[5] = {0.2,0.2,0.2,0.2,0.2}; */
/*   double lambda[5] = {0.21,0.21,0.21,0.21,0.21}; */
  
  /* Energy corresponding to the Gauge part */
  double new_plaquette_energy=0., new_rectangle_energy = 0., gauge_energy = 0., new_gauge_energy = 0.;
  double ret_plaquette_energy=0., ret_rectangle_energy = 0., ret_gauge_energy = 0.;

  /* Energy corresponding to the Momenta part */
  double enep=0., enepx=0., ret_enep = 0.;

  /* Energy corresponding to the pseudo fermion part(s) */
  double enerphi0 =0., enerphi0x =0., enerphi1 =0., enerphi1x =0., enerphi2 = 0., enerphi2x = 0.;
  double ret_enerphi0 = 0., ret_enerphi1 = 0., ret_enerphi2 = 0.;
  FILE * rlxdfile=NULL, * datafile=NULL, * ret_check_file=NULL;


   Ener[0] = 0;
   factor[0] = 1.0;
   for(j=1; j<8; j++){
     factor[j] = j*factor[j-1];
     Ener[j] = 0;
   }

  if(ini_g_tmp == 0) {
    ini_g_tmp = 1;
    init_gauge_tmp(VOLUME);
  }

  /* This is needed in order to let the */
  /* extended version of leap frog and  */
  /* Sexton-Weingarten work also with   */
  /* only one pseudo fermion field      */
  if(g_nr_of_psf == 1) {
    halfstep = 1;
  }

  /* For chronological inverter */
  g_csg_N[1] = 0; g_csg_N[3] = 0; g_csg_N[5] = 0; g_csg_N[7] = 0;

#ifdef MPI
  atime = MPI_Wtime();
#endif

  /* copy the gauge field to gauge_tmp */
  dontdump = 1;
  for(ix=0;ix<VOLUME;ix++) { 
    for(mu=0;mu<4;mu++) {
      v=&g_gauge_field[ix][mu];
      w=&gauge_tmp[ix][mu];
      _su3_assign(*w,*v);
    }
  }
  dontdump = 0;
  if(forcedump == 1) {
    write_gauge_field_time_p("last_configuration");
    if(g_proc_id==0) {
      rlxd_get(rlxd_state);
      rlxdfile=fopen("last_state","w");
      fwrite(rlxd_state,sizeof(rlxd_state),1,rlxdfile);
      fclose(rlxdfile);
    }
    exit(0);
  }
  
  /* initialize the pseudo-fermion fields    */
  /* depending on g_mu1 and g_mu2 we use     */
  /* one or two pseudo-fermion fields        */
  random_spinor_field(g_spinor_field[2], VOLUME/2);
  /* compute the square of the norm */
  enerphi0 = square_norm(g_spinor_field[2], VOLUME/2);
  if(g_proc_id == g_stdio_proc) printf(" Start HMC Energy = %e \n", enerphi0);



  /* IF PHMC */

  /* Here come the definition of the "heavy" 2-flavoured pseudofermion */

  random_spinor_field(g_chi_up_spinor_field[0], VOLUME/2);
  enerphi0 += square_norm(g_chi_up_spinor_field[0], VOLUME/2);

  if(g_proc_id == g_stdio_proc){
    printf(" Here comes the computation of H_old with \n \n");
    printf(" First: random spinors and their norm  \n ");
    printf(" OLD Ennergy UP %e \n", enerphi0);
  }

  random_spinor_field(g_chi_dn_spinor_field[0], VOLUME/2);
  enerphi0 += square_norm(g_chi_dn_spinor_field[0], VOLUME/2);

  if(g_proc_id == g_stdio_proc){
    printf(" OLD Energy  DN + UP %e \n\n", enerphi0);
  }

  QNon_degenerate(g_chi_up_spinor_field[1], g_chi_dn_spinor_field[1], g_chi_up_spinor_field[0], g_chi_dn_spinor_field[0]);
 
  for(j=1; j<=(dop_n_cheby-1); j++){
    assign(g_chi_up_spinor_field[0], g_chi_up_spinor_field[1], VOLUME/2);
    assign(g_chi_dn_spinor_field[0], g_chi_dn_spinor_field[1], VOLUME/2);

    L_POLY_MIN_CCONST(g_chi_up_spinor_field[1], g_chi_dn_spinor_field[1], g_chi_up_spinor_field[0], g_chi_dn_spinor_field[0], roo[dop_n_cheby-2+j]);
  }


  Poly_tilde_ND(g_chi_up_spinor_field[0], g_chi_dn_spinor_field[0], ptilde_cheby_coef, ptilde_n_cheby, g_chi_up_spinor_field[1], g_chi_dn_spinor_field[1]);;


  assign(g_chi_up_copy, g_chi_up_spinor_field[0], VOLUME/2);
  assign(g_chi_dn_copy, g_chi_dn_spinor_field[0], VOLUME/2);


  temp = square_norm(g_chi_up_spinor_field[0], VOLUME/2);
  if(g_proc_id == g_stdio_proc){
    printf(" Then: evaluate Norm of pseudofermion heatbath BHB \n ");
    printf(" Norm of BHB up squared %e \n", temp);
  }
  temp += square_norm(g_chi_dn_spinor_field[0], VOLUME/2);
  if(g_proc_id == g_stdio_proc){
    printf(" Norm of BHB up + BHB dn squared %e \n\n", temp);
  }

  /* END IF PHMC */



  if(g_nr_of_psf > 1) {
    random_spinor_field(g_spinor_field[3], VOLUME/2);
    enerphi1 = square_norm(g_spinor_field[3], VOLUME/2);
  }
  if(g_nr_of_psf > 2) {
    random_spinor_field(g_spinor_field[5], VOLUME/2);
    enerphi2 = square_norm(g_spinor_field[5], VOLUME/2);
  }
  /* apply the fermion matrix to the first spinor */
  /* it has the largest mu available              */
  g_mu = g_mu1;
  Qtm_plus_psi(g_spinor_field[first_psf], g_spinor_field[2]);
  chrono_add_solution(g_spinor_field[first_psf], g_csg_field[0], g_csg_index_array[0],
		      g_csg_N[0], &g_csg_N[1], VOLUME/2);
  if(g_nr_of_psf == 1 && ITER_MAX_BCG > 0 && fabs(g_mu1) == 0.) {
      chrono_add_solution(g_spinor_field[first_psf], g_csg_field[1], g_csg_index_array[1],
			  g_csg_N[2], &g_csg_N[3], VOLUME/2);
    }

  /* contruct the second \phi_o */
  if(g_nr_of_psf > 1) {
    g_mu = g_mu2;
    Qtm_plus_psi(g_spinor_field[3], g_spinor_field[3]);
    g_mu = g_mu1;
    zero_spinor_field(g_spinor_field[second_psf],VOLUME/2);
    if(fabs(g_mu)>0.) ITER_MAX_BCG = 0;
    idis1 = bicg(second_psf, 3, g_eps_sq_acc1, g_relative_precision_flag);
    ITER_MAX_BCG = saveiter_max;
    chrono_add_solution(g_spinor_field[second_psf], g_csg_field[1], g_csg_index_array[1],
			g_csg_N[2], &g_csg_N[3], VOLUME/2);
    if(g_nr_of_psf == 2 && ITER_MAX_BCG > 0 && fabs(g_mu2) == 0.) {
      chrono_add_solution(g_spinor_field[second_psf], g_csg_field[2], g_csg_index_array[2],
			  g_csg_N[4], &g_csg_N[5], VOLUME/2);
    }
  }
  /* contruct the third \phi_o */
  if(g_nr_of_psf > 2) {
    g_mu = g_mu3;
    Qtm_plus_psi(g_spinor_field[5], g_spinor_field[5]);
    g_mu = g_mu2;
    zero_spinor_field(g_spinor_field[third_psf],VOLUME/2);
    if(fabs(g_mu)>0.) ITER_MAX_BCG = 0;
    idis2 = bicg(third_psf, 5, g_eps_sq_acc2, g_relative_precision_flag);
    ITER_MAX_BCG = saveiter_max;
    chrono_add_solution(g_spinor_field[third_psf], g_csg_field[2], g_csg_index_array[2],
			g_csg_N[4], &g_csg_N[5], VOLUME/2);
    if(ITER_MAX_BCG > 0 && fabs(g_mu3) == 0.) {
      chrono_add_solution(g_spinor_field[third_psf], g_csg_field[3], g_csg_index_array[3],
			  g_csg_N[6], &g_csg_N[7], VOLUME/2);
    }
  }


  /* initialize the momenta */
  enep=ini_momenta();

  /*run the trajectory*/
  if(integtyp == 1) {
    /* Leap-frog integration scheme */

    /* COMMENTED IF PHMC used 
    leap_frog(dtau, Nsteps, nsmall); 
    */    

    /* IF PHMC */
    if(g_proc_id == g_stdio_proc) printf(" Here comes the NEW leap-frog integration \n \n");

    leap_frog_ND(dtau, Nsteps, nsmall); 
  }
  else if(integtyp == 2) {
    /* Sexton Weingarten integration scheme */
    sexton(dtau, Nsteps, nsmall);
  }
  else if(integtyp == 3) {
    ext_leap_frog(n_int, tau, g_nr_of_psf, halfstep);
  }
  else if(integtyp == 4) {
    ext_sexton_weingarten(n_int, tau, g_nr_of_psf, halfstep);
  }
  else if(integtyp == 5) {
    impr_leap_frog(n_int, tau, g_nr_of_psf);
  }
  else if(integtyp == 6) {
    mn2_integrator(n_int, tau, g_nr_of_psf, halfstep, lambda);
  }
  else if(integtyp == 7) {
    mn2p_integrator(n_int, tau, g_nr_of_psf, lambda);
  }

  /*perform the accept-reject-step*/
  enepx=moment_energy();
  new_plaquette_energy=measure_gauge_action();
  if(g_rgi_C1 > 0. || g_rgi_C1 < 0.) {
    new_rectangle_energy = measure_rectangles();
  }
  gauge_energy = g_rgi_C0 * (*plaquette_energy) + g_rgi_C1 * (*rectangle_energy);
  new_gauge_energy = g_rgi_C0 * new_plaquette_energy + g_rgi_C1 * new_rectangle_energy;

  /* compute the energy contributions from the pseudo-fermions */
  g_mu = g_mu1;
  if(fabs(g_mu)>0.) ITER_MAX_BCG = 0;
  chrono_guess(g_spinor_field[2], g_spinor_field[first_psf], g_csg_field[0], g_csg_index_array[0],
	       g_csg_N[0], g_csg_N[1], VOLUME/2, &Qtm_pm_psi);
  idis0=bicg(2, first_psf, g_eps_sq_acc1, g_relative_precision_flag);
  ITER_MAX_BCG = saveiter_max;
  /* Save the solution of Q^-2 at the right place */
  /* for later reuse! */
  assign(g_spinor_field[DUM_DERI+4], g_spinor_field[DUM_DERI+6], VOLUME/2);
  /* Compute the energy contr. from first field */
  enerphi0x = square_norm(g_spinor_field[2], VOLUME/2);
  if(g_proc_id == g_stdio_proc) printf(" Final HMC Energy = %e \n", enerphi0x);


  /* IF PHMC */

  /* This is needed if we consider only "1" in eq. 9 */
  assign(g_chi_up_spinor_field[1], g_chi_up_copy, VOLUME/2);
  assign(g_chi_dn_spinor_field[1], g_chi_dn_copy,  VOLUME/2);

  for(j=1; j<=(dop_n_cheby-1); j++){
    assign(g_chi_up_spinor_field[0], g_chi_up_spinor_field[1], VOLUME/2);
    assign(g_chi_dn_spinor_field[0], g_chi_dn_spinor_field[1], VOLUME/2);

    L_POLY_MIN_CCONST(g_chi_up_spinor_field[1], g_chi_dn_spinor_field[1], g_chi_up_spinor_field[0], g_chi_dn_spinor_field[0], roo[j-1]);
  }
  
  ij=0;
  assign(g_chi_up_spinor_field[ij], g_chi_up_spinor_field[1], VOLUME/2);
  assign(g_chi_dn_spinor_field[ij], g_chi_dn_spinor_field[1], VOLUME/2);

  temp = square_norm(g_chi_up_spinor_field[ij], VOLUME/2);
  Ener[ij] = temp;

  temp = square_norm(g_chi_dn_spinor_field[ij], VOLUME/2);
  Ener[ij] += temp;

  if(g_proc_id == g_stdio_proc){
    printf(" Here comes the computation of H_new with \n \n");

    printf(" At j=%d  P+HMC Final Energy %e \n", ij, enerphi0x+Ener[ij]);
    printf(" At j=%d  PHMC Only Final Energy %e \n", ij, Ener[ij]);
  }

  /* Here comes the loop for the evaluation of A, A^2, ...  */
  for(j=1; j<1; j++){ /* To omit corrections just set  j<1 */

    if(j % 2){ /*  Chi[j] = ( Qdag P  Ptilde ) Chi[j-1]  */ 
      Poly_tilde_ND(g_chi_up_spinor_field[j], g_chi_dn_spinor_field[j], ptilde_cheby_coef, ptilde_n_cheby, g_chi_up_spinor_field[j-1], g_chi_dn_spinor_field[j-1]);
      QdaggerQ_poly(g_chi_up_spinor_field[j-1], g_chi_dn_spinor_field[j-1], dop_cheby_coef, dop_n_cheby, g_chi_up_spinor_field[j], g_chi_dn_spinor_field[j]);
      QdaggerNon_degenerate(g_chi_up_spinor_field[j], g_chi_dn_spinor_field[j], g_chi_up_spinor_field[j-1], g_chi_dn_spinor_field[j-1]);
    }
    else{ /*  Chi[j] = ( Ptilde P Q ) Chi[j-1]  */ 
      QNon_degenerate(g_chi_up_spinor_field[j], g_chi_dn_spinor_field[j], g_chi_up_spinor_field[j-1], g_chi_dn_spinor_field[j-1]);
      QdaggerQ_poly(g_chi_up_spinor_field[j-1], g_chi_dn_spinor_field[j-1], dop_cheby_coef, dop_n_cheby, g_chi_up_spinor_field[j], g_chi_dn_spinor_field[j]);
      Poly_tilde_ND(g_chi_up_spinor_field[j], g_chi_dn_spinor_field[j], ptilde_cheby_coef, ptilde_n_cheby, g_chi_up_spinor_field[j-1], g_chi_dn_spinor_field[j-1]);
    }

    Ener[j] = Ener[j-1] + Ener[0];
    sgn = -1.0;
    for(ij=1; ij<j; ij++){
      fact = factor[j] / (factor[ij] * factor[j-ij]);
      printf(" Here  j=%d  and  ij=%d   sign=%f  fact=%f \n", j ,ij, sgn, fact);
      Ener[j] += sgn*fact*Ener[ij];
      sgn = (-1)*sgn;
    }
    temp = square_norm(g_chi_up_spinor_field[j], VOLUME/2);
    temp += square_norm(g_chi_dn_spinor_field[j], VOLUME/2);
    printf(" Here  j=%d   sign=%f  temp=%e \n", j, sgn, temp);

    Ener[j] += sgn*temp;

    Diff = fabs(Ener[j] - Ener[j-1]);
    printf(" At j=%d  Energy=%e  Diff=%e \n", j, Ener[j], Diff);

    if(Diff < 1.0e-4){   
      printf(" At j = %d  PHMC Only Final Energy %e \n", j, Ener[j]);
      break;
    }

  }


  enerphi0x += Ener[ij];
  if(g_proc_id == g_stdio_proc) printf(" At j = %d  P=%e +HMC Final Energy %e \n\n", ij, Ener[ij], enerphi0x);
  
  /* END IF PHMC */


  if(g_nr_of_psf > 1) {
    g_mu = g_mu1;
    Qtm_plus_psi(g_spinor_field[DUM_DERI+5], g_spinor_field[second_psf]);
    g_mu = g_mu2;
    if(fabs(g_mu)>0.) ITER_MAX_BCG = 0;
    chrono_guess(g_spinor_field[3], g_spinor_field[DUM_DERI+5], g_csg_field[1], g_csg_index_array[1],
		 g_csg_N[2], g_csg_N[3], VOLUME/2, &Qtm_pm_psi);
    idis1 += bicg(3, DUM_DERI+5, g_eps_sq_acc2, g_relative_precision_flag); 
    ITER_MAX_BCG = saveiter_max;
    /* Compute the energy contr. from second field */
    enerphi1x = square_norm(g_spinor_field[3], VOLUME/2);
  }
  if(g_nr_of_psf > 2) {
    g_mu = g_mu2;
    Qtm_plus_psi(g_spinor_field[DUM_DERI+6], g_spinor_field[third_psf]);
    g_mu = g_mu3;
    if(fabs(g_mu)>0.) ITER_MAX_BCG = 0;
    chrono_guess(g_spinor_field[5], g_spinor_field[DUM_DERI+6], g_csg_field[2], g_csg_index_array[2],
		 g_csg_N[4], g_csg_N[5], VOLUME/2, &Qtm_pm_psi);
    idis2 += bicg(5, DUM_DERI+6, g_eps_sq_acc3, g_relative_precision_flag);
    ITER_MAX_BCG = saveiter_max;
    /* Compute the energy contr. from third field */
    enerphi2x = square_norm(g_spinor_field[5], VOLUME/2);
  }

  if(g_proc_id == g_stdio_proc){
    printf(" Energies: Old = %f New = %f   Exp(-Delta H_heavy) = %f \n", enerphi0, enerphi0x, exp(enerphi0-enerphi0x));

    printf(" \n Contributions to the Hamiltonian due to \n");
    printf(" Pi=%f NewPi=%f   Gauge=%f NewGauge=%f \n", enep, enepx, g_beta*gauge_energy, g_beta*new_gauge_energy);
    printf(" BHB1=%f NewBHB1=%f  \n", enerphi0, enerphi0x);
    printf(" BHB2=%f NewBHB2=%f   BHB3=%f NewBHB3=%f \n", enerphi1, enerphi1x, enerphi2, enerphi2x);
  }

  /* Compute the energy difference */
  dh= +enepx - g_beta*new_gauge_energy - enep + g_beta*gauge_energy
      + enerphi0x - enerphi0 + enerphi1x - enerphi1 + enerphi2x - enerphi2; 
  expmdh = exp(-dh);

  if(g_proc_id == g_stdio_proc) printf("  Exp(-Delta H) = %f \n \n", expmdh);
      
  /* the random number is only taken at node zero and then distributed to 
     the other sites */
  if(g_proc_id==0) {
    ranlxd(yy,1);
#ifdef MPI
    for(i = 1; i < g_nproc; i++) {
      MPI_Send(&yy[0], 1, MPI_DOUBLE, i, 31, MPI_COMM_WORLD);
    }
#endif
  }
#ifdef MPI
  else{
    MPI_Recv(&yy[0], 1, MPI_DOUBLE, 0, 31, MPI_COMM_WORLD, &status);
  }
#endif

  if(expmdh > yy[0]) {
    accept = 1;
    if(g_proc_id == g_stdio_proc) printf("\n \n !!!!!!!   IT ACCEPTED HERE   !!!!!!!! \n \n");

  }
  else {
    accept = 0;
  }

  /* REWEIGHTING */
  /* Argument = number of rnd spinor to be used */ 
  /*
  reweighting_factor_nd(10);
  */


  /* Here a reversibility test is performed */
  /* The trajectory is integrated back      */
  if(return_check == 1) {
    if(accept == 1) {
      write_gauge_field_time_p( "conf.save" );
    }

    /* run the trajectory back */
    if(integtyp == 1) {
      /* Leap-frog integration scheme */
      /* COMMENTED IF PHMC used 
      leap_frog(-dtau, Nsteps, nsmall); 
      */      

      /* IF PHMC */
      leap_frog_ND(-dtau, Nsteps, nsmall); 
      
    }
    else if(integtyp == 2) {
      /* Sexton Weingarten integration scheme */
      sexton(-dtau, Nsteps, nsmall);
    }
    else if(integtyp == 3) {
      ext_leap_frog(n_int, -tau, g_nr_of_psf, halfstep);
    }
    else if(integtyp == 4) {
      ext_sexton_weingarten(n_int, -tau, g_nr_of_psf, halfstep);
    }
    else if(integtyp == 5) {
      impr_leap_frog(n_int, -tau, g_nr_of_psf);
    }
    else if(integtyp == 6) {
      mn2_integrator(n_int, -tau, g_nr_of_psf, halfstep, lambda);
    }
    else if(integtyp == 7) {
      mn2p_integrator(n_int, -tau, g_nr_of_psf, lambda);
    }

    ret_enep=moment_energy();
    ret_plaquette_energy=measure_gauge_action();
    if(g_rgi_C1 > 0. || g_rgi_C1 < 0.) {
      ret_rectangle_energy = measure_rectangles();
    }
    ret_gauge_energy = g_rgi_C0 * ret_plaquette_energy + g_rgi_C1 * ret_rectangle_energy;
    
    /*compute the energy contributions from the pseudo-fermions */
    assign(g_spinor_field[2], g_spinor_field[DUM_DERI+4], VOLUME/2);
    g_mu = g_mu1;
    if(fabs(g_mu)>0.) ITER_MAX_BCG = 0;
    ret_idis0=bicg(2, first_psf, g_eps_sq_acc, g_relative_precision_flag);
    ITER_MAX_BCG = saveiter_max;
    assign(g_spinor_field[DUM_DERI+4], g_spinor_field[DUM_DERI+6], VOLUME/2);
    
    ret_enerphi0=square_norm(g_spinor_field[2], VOLUME/2);
    if(g_nr_of_psf > 1) {
      assign(g_spinor_field[3], g_spinor_field[DUM_DERI+5], VOLUME/2);
      g_mu = g_mu1;
      Qtm_plus_psi(g_spinor_field[second_psf], g_spinor_field[second_psf]);
      g_mu = g_mu2;
      if(fabs(g_mu)>0.) ITER_MAX_BCG = 0;
      ret_idis1 += bicg(3, second_psf, g_eps_sq_acc, g_relative_precision_flag);
      ITER_MAX_BCG = saveiter_max;
      assign(g_spinor_field[DUM_DERI+5], g_spinor_field[DUM_DERI+6], VOLUME/2);
      ret_enerphi1 = square_norm(g_spinor_field[3], VOLUME/2);
    }
    if(g_nr_of_psf > 2) {
      assign(g_spinor_field[5], g_spinor_field[DUM_DERI+6], VOLUME/2);
      g_mu = g_mu2;
      Qtm_plus_psi(g_spinor_field[third_psf], g_spinor_field[third_psf]);
      g_mu = g_mu3;
      if(fabs(g_mu)>0.) ITER_MAX_BCG = 0;
      ret_idis2 += bicg(5, third_psf, g_eps_sq_acc, g_relative_precision_flag);
      ITER_MAX_BCG = saveiter_max;
      ret_enerphi2 = square_norm(g_spinor_field[5], VOLUME/2);
    }
    
    /* Compute the energy difference */
    ret_dh= +ret_enep - g_beta*ret_gauge_energy - enep + g_beta*gauge_energy
      + ret_enerphi0 - enerphi0 + ret_enerphi1 - enerphi1 + ret_enerphi2 - enerphi2;

    /* Output */
    if(g_proc_id == 0) {
      ret_check_file = fopen("return_check.data","a");
      fprintf(ret_check_file,"dh = %e, \n",ret_dh);
      fclose(ret_check_file);
    }

    if(accept == 1) {
      read_gauge_field_time_p( "conf.save" );
    }
  }

  if(accept == 1) {
    /* accept */
    (*plaquette_energy)=new_plaquette_energy;
    (*rectangle_energy)=new_rectangle_energy;
    dontdump = 1;
    /* put the links back to SU(3) group */
    for(ix=0;ix<VOLUME;ix++) { 
      for(mu=0;mu<4;mu++) { 
	/* this is MIST */
	v=&g_gauge_field[ix][mu];
	*v=restoresu3(*v); 
      }
    }
  }
  else {
    /* reject: copy gauge_tmp to g_gauge_field */
    for(ix=0;ix<VOLUME;ix++) {
      for(mu=0;mu<4;mu++){
	/* Auch MIST */
	v=&g_gauge_field[ix][mu];
	w=&gauge_tmp[ix][mu];
	_su3_assign(*v,*w);
      }
    }
  }
#ifdef MPI
  xchange_gauge();
  etime = MPI_Wtime();
#endif
#ifdef _GAUGE_COPY
  update_backward_gauge();
#endif

  if(g_proc_id==0){
    datafile = fopen(filename, "a");
    fprintf(datafile,"%14.12f %14.12f %e %d %d %d ",
	    (*plaquette_energy)/(6.*VOLUME*g_nproc),dh,expmdh,
	    idis0, count00, count01);
    if(g_nr_of_psf > 1) {
      fprintf(datafile, "%d %d %d ", idis1, count10, count11);
    }
    if(g_nr_of_psf > 2) {
      fprintf(datafile, "%d %d %d ", idis2, count20, count21);
    }
    fprintf(datafile, "%d %e", accept, etime-atime);
    if(g_rgi_C1 > 0. || g_rgi_C1 < 0.) {
      fprintf(datafile, " %e", (*rectangle_energy)/(12*VOLUME*g_nproc));
    }
    fprintf(datafile, "\n");
    fflush(datafile);
    fclose(datafile);
  }

  return(accept);
}

static char const rcsid[] = "$Id$";