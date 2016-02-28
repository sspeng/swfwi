
/* Time domain full waveform inversion
Note: This serial FWI is merely designed to help the understanding of
beginners. Enquist absorbing boundary condition (A2) is applied!
 */
/*
  Copyright (C) 2014  Xi'an Jiaotong University, UT Austin (Pengliang Yang)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  Important references:
    [1] Clayton, Robert, and Björn Engquist. "Absorbing boundary
  conditions for acoustic and elastic wave equations." Bulletin
  of the Seismological Society of America 67.6 (1977): 1529-1540.
    [2] Tarantola, Albert. "Inversion of seismic reflection data in the
  acoustic approximation." Geophysics 49.8 (1984): 1259-1266.
    [3] Pica, A., J. P. Diet, and A. Tarantola. "Nonlinear inversion
  of seismic reflection data in a laterally invariant medium."
  Geophysics 55.3 (1990): 284-292.
    [4] Dussaud, E., Symes, W. W., Williamson, P., Lemaistre, L.,
  Singer, P., Denel, B., & Cherrett, A. (2008). Computational
  strategies for reverse-time migration. In SEG Technical Program
  Expanded Abstracts 2008 (pp. 2267-2271).
    [5] Hager, William W., and Hongchao Zhang. "A survey of nonlinear
  conjugate gradient methods." Pacific journal of Optimization
  2.1 (2006): 35-58.
 */

extern "C"
{
#include <rsf.h>
}

#include <mpi.h>
#include <time.h>
#include <cmath>

#include <omp.h>
#include <algorithm>
#include <numeric>
#include <cstdlib>
#include <vector>

#include <boost/timer/timer.hpp>
#include "logger.h"
#include "mpi-fwi-params.h"
#include "common.h"
#include "ricker-wavelet.h"
#include "cycle-swap.h"
#include "sum.h"
#include "mpi-utility.h"
#include "sf-velocity-reader.h"
#include "shotdata-reader.h"

float cal_obj_derr_illum_grad(const MpiFwiParams &params,
    float *derr,  /* output */
    float *illum, /* output */
    float *g1,    /* output */
    const float *vv,
    const float *wlt,
    const float *dobs,
    const int *sxz,
    const int *gxz)
{
  int size = params.numProc;
  int rank = params.rank;
  int nt = params.nt;
  int nz = params.nz;
  int nx = params.nx;
  int ng = params.ng;
  int ns = params.ns;
  int nk = params.nk;
  float dt = params.dt;
  float dx = params.dx;
  float dz = params.dz;

  std::vector<float> bndr(nt * (2 * nz + nx), 0); /* boundaries for wavefield reconstruction */
  std::vector<float> dcal(ng, 0); /* calculated/synthetic seismic data */

  std::vector<float> sp0(nz * nx); /* source wavefield p0 */
  std::vector<float> sp1(nz * nx); /* source wavefield p1 */
  std::vector<float> sp2(nz * nx); /* source wavefield p2 */
  std::vector<float> gp0(nz * nx); /* geophone/receiver wavefield p0 */
  std::vector<float> gp1(nz * nx); /* geophone/receiver wavefield p1 */
  std::vector<float> gp2(nz * nx); /* geophone/receiver wavefield p2 */
  std::vector<float> lap(nz * nx); /* laplace of the source wavefield */

  float dtx = dt / dx;
  float dtz = dt / dz;
  int nb = 0; // no expanded boundary

  for (int is = rank, ik = 0; is < ns; is += size, ik++) {
    std::fill(sp0.begin(), sp0.end(), 0);
    std::fill(sp1.begin(), sp1.end(), 0);
    for (int it = 0; it < nt; it++) {
      add_source(&sp1[0], &wlt[it], &sxz[is], 1, nz, nb, true);
      step_forward(&sp0[0], &sp1[0], &sp2[0], vv, dtz, dtx, nz, nx);
      // cycle swap
      cycleSwap(sp0, sp1, sp2);

      rw_bndr(&bndr[it * (2 * nz + nx)], &sp0[0], nz, nx, true);

      record_seis(&dcal[0], gxz, &sp0[0], ng, nz, nb);
      cal_residuals(&dcal[0], &dobs[ik * nt * ng + it * ng], &derr[ik * ng * nt + it * ng], ng);
    }

    std::swap(sp0, sp1);

    std::fill(gp0.begin(), gp0.end(), 0);
    std::fill(gp1.begin(), gp1.end(), 0);

    for (int it = nt - 1; it > -1; it--) {
      rw_bndr(&bndr[it * (2 * nz + nx)], &sp1[0], nz, nx, false);
      step_backward(illum, &lap[0], &sp0[0], &sp1[0], &sp2[0], vv, dtz, dtx, nz, nx);
      add_source(&sp1[0], &wlt[it], &sxz[is], 1, nz, nb, false);

      add_source(&gp1[0], &derr[ik * ng * nt + it * ng], gxz, ng, nz, nb, true);
      step_forward(&gp0[0], &gp1[0], &gp2[0], vv, dtz, dtx, nz, nx);

      cal_gradient(&g1[0], &lap[0], &gp1[0], nz, nx);

      cycleSwap(sp0, sp1, sp2);
      cycleSwap(gp0, gp1, gp2);
    }

  } /// output: derr, g1, illum

  float obj = cal_objective(&derr[0], ng * nt * nk);

  return obj;
}

float calVelUpdateStepLen(const MpiFwiParams &params,
    const float *vtmp,
    const float *wlt,
    const float *dobs,
    const int *sxz,
    const int *gxz,
    const float *derr,
    float epsil
    )
{
  const int size = params.numProc;
  const int rank = params.rank;
  int nt = params.nt;
  int nz = params.nz;
  int nx = params.nx;
  int ng = params.ng;
  int ns = params.ns;
  float dt = params.dt;
  float dx = params.dx;
  float dz = params.dz;

  std::vector<float> dcal(ng, 0); /* calculated/synthetic seismic data */
  std::vector<float> sp0(nz * nx); /* source wavefield p0 */
  std::vector<float> sp1(nz * nx); /* source wavefield p1 */
  std::vector<float> sp2(nz * nx); /* source wavefield p2 */

  std::vector<float> alpha1(ng, 0); /* numerator of alpha, length=ng */
  std::vector<float> alpha2(ng, 0); /* denominator of alpha, length=ng */

  float dtx = dt / dx;
  float dtz = dt / dz;
  int nb = 0; // no expanded boundary

  for (int is = rank, ik = 0; is < ns; is+= size, ik++) {
    std::fill(sp0.begin(), sp0.end(), 0);
    std::fill(sp1.begin(), sp1.end(), 0);
    for (int it = 0; it < nt; it++) {
      add_source(&sp1[0], &wlt[it], &sxz[is], 1, nz, nb, true);
      step_forward(&sp0[0], &sp1[0], &sp2[0], &vtmp[0], dtz, dtx, nz, nx);

      std::swap(sp0, sp1);
      std::swap(sp1, sp2);

      record_seis(&dcal[0], gxz, &sp0[0], ng, nz, nb);
      sum_alpha12(&alpha1[0], &alpha2[0], &dcal[0], &dobs[ik * nt * ng + it * ng], &derr[ik * ng * nt + it * ng], ng);
    }
  }


  /**
   * MPI reduce
   */
  MpiInplaceReduce(&alpha1[0], ng, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
  MpiInplaceReduce(&alpha2[0], ng, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);

  float alpha = cal_alpha(&alpha1[0], &alpha2[0], epsil, ng);

  return alpha;
}

int main(int argc, char *argv[]) {
  MPI_Init (&argc, &argv);

  /* initialize Madagascar */
  sf_init(argc, argv);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);/* who am I? */

//  Logger::instance().init("mpi-fwi", boost::log::trivial::debug, Logger::NO_TIMESTAMP, rank);
  Logger::instance().init("mpi-fwi");

  MpiFwiParams &params = MpiFwiParams::instance();

  // how many groups of MPI chunk
  INFO() << format("each process should process %d shots") % params.nk;

  std::vector<float> vv(params.nz * params.nx, 0);    /* updated velocity */
  std::vector<float> dobs(params.nk * params.nt * params.ng); /* observed data */
  std::vector<float> cg(params.nz * params.nx, 0);    /* conjugate gradient */
  std::vector<float> g0(params.nz * params.nx, 0);    /* gradient at previous step */
  std::vector<float> wlt(params.nt); /* ricker wavelet */
  std::vector<int> sxz(params.ns); /* source positions */
  std::vector<int> gxz(params.ng); /* geophone positions */
  std::vector<float> objval(params.niter, 0); /* objective/misfit function */

  /* initialize varibles */
  rickerWavelet(&wlt[0], params.nt, params.fm, params.dt);
  sg_init(&sxz[0], params.szbeg, params.sxbeg, params.jsz, params.jsx, params.ns, params.nz);
  sg_init(&gxz[0], params.gzbeg, params.gxbeg, params.jgz, params.jgx, params.ng, params.nz);

  // read velocity
  SfVelocityReader velReader(params.vinit);
  velReader.readAndBcast(&vv[0], vv.size(), rank);

  // read observed data
  ShotDataReader::parallelRead(params.obsDataFileName, &dobs[0], params.ns, params.nt, params.ng);

  float obj0 = 0;
  for (int iter = 0; iter < params.niter; iter++) {
    boost::timer::cpu_timer timer;
    std::vector<float> g1(params.nz * params.nx, 0);    /* gradient at curret step */
    std::vector<float> derr(params.nk * params.ng * params.nt, 0); /* residual/error between synthetic and observation */
    std::vector<float> illum(params.nz * params.nx, 0); /* illumination of the source wavefield */
    std::vector<float> vtmp(params.nz * params.nx, 0);  /* temporary velocity computed with epsil */

    /**
     * calculate local objective function & derr & illum & g1(gradient)
     */
    float obj = cal_obj_derr_illum_grad(params, &derr[0], &illum[0], &g1[0], &vv[0], &wlt[0], &dobs[0], &sxz[0], &gxz[0]);

    /* MPI reduce */
    MpiInplaceReduce(&obj, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
    MpiInplaceReduce(&g1[0], params.nz * params.nx, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
    MpiInplaceReduce(&illum[0], params.nz * params.nx, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) DEBUG() << format("sum_derr %f, sum_illum %f, sum_g1 %f") % sum(derr) % sum(illum) % sum(g1);

    objval[iter] = iter == 0 ? obj0 = obj, 1.0 : obj / obj0;

    float epsil = 0;
    float beta = 0;
    if (rank == 0) {
      sf_floatwrite(&illum[0], params.nz * params.nx, params.illums);

      scale_gradient(&g1[0], &vv[0], &illum[0], params.nz, params.nx, params.precon);
      bell_smoothz(&g1[0], &illum[0], params.rbell, params.nz, params.nx);
      bell_smoothx(&illum[0], &g1[0], params.rbell, params.nz, params.nx);
      sf_floatwrite(&g1[0], params.nz * params.nx, params.grads);

      DEBUG() << format("before beta: sum_g0: %f, sum_g1: %f, sum_cg: %f") % sum(g0) % sum(g1) % sum(cg);
      beta = iter == 0 ? 0.0 : cal_beta(&g0[0], &g1[0], &cg[0], params.nz, params.nx);

      cal_conjgrad(&g1[0], &cg[0], beta, params.nz, params.nx);
      epsil = cal_epsilon(&vv[0], &cg[0], params.nz, params.nx);
      cal_vtmp(&vtmp[0], &vv[0], &cg[0], epsil, params.nz, params.nx);

      std::swap(g1, g0); // let g0 be the previous gradient

    }

    MPI_Bcast(&vtmp[0], params.nz * params.nx, MPI_FLOAT, 0, MPI_COMM_WORLD);

    float alpha = calVelUpdateStepLen(params, &vtmp[0], &wlt[0], &dobs[0], &sxz[0], &gxz[0], &derr[0], epsil);

    if (rank == 0) {
      update_vel(&vv[0], &cg[0], alpha, params.nz, params.nx);

      sf_floatwrite(&vv[0], params.nz * params.nx, params.vupdates);

      // output important information at each FWI iteration
      INFO() << format("iteration %d obj=%f  beta=%f  epsil=%f  alpha=%f") % (iter + 1) % obj % beta % epsil % alpha;
//      INFO() << timer.format(2);
    } // end of rank 0

    MPI_Bcast(&vv[0], params.nz * params.nx, MPI_FLOAT, 0, MPI_COMM_WORLD);
  } /// end of iteration

  if (rank == 0) {
    sf_floatwrite(&objval[0], params.niter, params.objs);
  }

  sf_close();
  MPI_Finalize();

  return 0;
}
