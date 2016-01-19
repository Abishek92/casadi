/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010-2014 Joel Andersson, Joris Gillis, Moritz Diehl,
 *                            K.U. Leuven. All rights reserved.
 *    Copyright (C) 2011-2014 Greg Horn
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "cplex_interface.hpp"
#include "casadi/core/std_vector_tools.hpp"
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>

#include "ilcplex/cplex.h"

namespace casadi {

  using namespace std;

  extern "C"
  int CASADI_QPSOL_CPLEX_EXPORT
  casadi_register_qpsol_cplex(Qpsol::Plugin* plugin) {
    plugin->creator = CplexInterface::creator;
    plugin->name = "cplex";
    plugin->doc = CplexInterface::meta_doc.c_str();
    plugin->version = 23;
    return 0;
  }

  extern "C"
  void CASADI_QPSOL_CPLEX_EXPORT casadi_load_qpsol_cplex() {
    Qpsol::registerPlugin(casadi_register_qpsol_cplex);
  }

  CplexInterface::CplexInterface(const std::string& name,
                                 const std::map<std::string, Sparsity>& st)
    : Qpsol(name, st) {
  }

  Options CplexInterface::options_
  = {{&Qpsol::options_},
     {{"qp_method",
       {OT_INT,
        "Determines which CPLEX algorithm to use."}},
      {"dump_to_file",
       {OT_BOOL,
        "Dumps QP to file in CPLEX format."}},
      {"dump_filename",
       {OT_STRING,
        "The filename to dump to."}},
      {"tol",
       {OT_DOUBLE,
        "Tolerance of solver"}},
      {"dep_check",
       {OT_INT,
        "Detect redundant constraints."}},
      {"simplex_maxiter",
       {OT_INT,
        "Maximum number of simplex iterations."}},
      {"barrier_maxiter",
       {OT_INT,
        "Maximum number of barrier iterations."}},
      {"warm_start",
       {OT_BOOL,
        "Use warm start with simplex methods (affects only the simplex methods)."}},
      {"convex",
       {OT_BOOL,
        "Indicates if the QP is convex or not (affects only the barrier method)."}}
     }
  };

  void CplexInterface::init(const Dict& opts) {
    // Call the init method of the base class
    Qpsol::init(opts);

    // Default options
    qp_method_ = 0;
    dump_to_file_ = false;
    dump_filename_ = "qp.dat";
    tol_ = 1e-6;
    dep_check_ = 0;
    simplex_maxiter_ = 2100000000;
    barrier_maxiter_ = 2100000000;
    warm_start_ = false;
    convex_ = true;

    // Read options
    for (auto&& op : opts) {
      if (op.first=="qp_method") {
        qp_method_ = op.second;
      } else if (op.first=="dump_to_file") {
        dump_to_file_ = op.second;
      } else if (op.first=="dump_filename") {
        dump_filename_ = op.second.to_string();
      } else if (op.first=="tol") {
        tol_ = op.second;
      } else if (op.first=="dep_check") {
        dep_check_ = op.second;
      } else if (op.first=="simplex_maxiter") {
        simplex_maxiter_ = op.second;
      } else if (op.first=="barrier_maxiter") {
        barrier_maxiter_ = op.second;
      } else if (op.first=="warm_start") {
        warm_start_ = op.second;
      } else if (op.first=="convex") {
        convex_ = op.second;
      }
    }

    // Allocate work vectors
    alloc_w(n_, true); // g
    alloc_w(n_, true); // lbx
    alloc_w(n_, true); // ubx
    alloc_w(nc_, true); // lba
    alloc_w(nc_, true); // uba
    alloc_w(nnz_in(QPSOL_H), true); // H
    alloc_w(nnz_in(QPSOL_A), true); // A
    alloc_w(n_, true); // x
    alloc_w(n_, true); // lam_x
    alloc_w(nc_, true); // lam_a
  }

  void CplexInterface::init_memory(Memory& mem) const {
    CplexMemory& m = dynamic_cast<CplexMemory&>(mem);

    int status;
    casadi_assert(m.env==0);
    m.env = CPXopenCPLEX(&status);
    casadi_assert_message(m.env!=0, "CPLEX: Cannot initialize CPLEX environment. STATUS: "
                          << status);

    // Turn on some debug messages if requested
    if (verbose()) {
      CPXsetintparam(m.env, CPX_PARAM_SCRIND, CPX_ON);
    } else {
      CPXsetintparam(m.env, CPX_PARAM_SCRIND, CPX_OFF);
    }
    if (status) {
      userOut() << "CPLEX: Problem with setting parameter... ERROR: " << status << std::endl;
    }

    /* SETTING OPTIONS */
    // Optimality tolerance
    status = CPXsetdblparam(m.env, CPX_PARAM_EPOPT, tol_);
    // Feasibility tolerance
    status = CPXsetdblparam(m.env, CPX_PARAM_EPRHS, tol_);
    // We start with barrier if crossover was chosen.
    if (qp_method_ == 7) {
      status = CPXsetintparam(m.env, CPX_PARAM_QPMETHOD, 4);
    } else { // Otherwise we just chose the algorithm
      status = CPXsetintparam(m.env, CPX_PARAM_QPMETHOD, qp_method_);
    }
    // Setting dependency check option
    status = CPXsetintparam(m.env, CPX_PARAM_DEPIND, dep_check_);
    // Setting barrier iteration limit
    status = CPXsetintparam(m.env, CPX_PARAM_BARITLIM, barrier_maxiter_);
    // Setting simplex iteration limit
    status = CPXsetintparam(m.env, CPX_PARAM_ITLIM, simplex_maxiter_);
    if (qp_method_ == 7) {
      // Setting crossover algorithm
      status = CPXsetintparam(m.env, CPX_PARAM_BARCROSSALG, 1);
    }
    if (!convex_) {
      // Enabling non-convex QPs
      status = CPXsetintparam(m.env, CPX_PARAM_SOLUTIONTARGET, CPX_SOLUTIONTARGET_FIRSTORDER);
    }

    // Exotic parameters, once they might become options...

    // Do careful numerics with numerically unstable problem
    //status = CPXsetintparam(m.env, CPX_PARAM_NUMERICALEMPHASIS, 1);
    // Set scaling approach
    //status = CPXsetintparam(m.env, CPX_PARAM_SCAIND, 1);
    // Set Markowitz tolerance
    //status = CPXsetdblparam(m.env, CPX_PARAM_EPMRK, 0.9);

    // Doing allocation of CPLEX data
    // Objective is to be minimized
    m.objsen = CPX_MIN;

    // Allocation of data
    // Type of constraint
    m.sense.resize(nc_);
    // Right-hand side of constraints
    m.rhs.resize(nc_);
    // Range value for lower AND  upper bounded constraints
    m.rngval.resize(nc_);
    // Basis for primal variables
    m.cstat.resize(n_);
    m.rstat.resize(nc_);

    // Matrix A, count the number of elements per column
    const Sparsity& A_sp = sparsity_in(QPSOL_A);
    m.matcnt.resize(A_sp.size2());
    transform(A_sp.colind()+1, A_sp.colind() + A_sp.size2()+1, A_sp.colind(), m.matcnt.begin(),
              minus<int>());

    // Matrix H, count the number of elements per column
    const Sparsity& H_sp = sparsity_in(QPSOL_H);
    m.qmatcnt.resize(H_sp.size2());
    transform(H_sp.colind()+1, H_sp.colind() + H_sp.size2()+1, H_sp.colind(), m.qmatcnt.begin(),
              minus<int>());

    casadi_assert(m.lp==0);
    m.lp = CPXcreateprob(m.env, &status, "QP from CasADi");
  }

  void CplexInterface::
  eval(Memory& mem, const double** arg, double** res, int* iw, double* w) const {
    CplexMemory& m = dynamic_cast<CplexMemory&>(mem);

    if (inputs_check_) {
      checkInputs(arg[QPSOL_LBX], arg[QPSOL_UBX], arg[QPSOL_LBA], arg[QPSOL_UBA]);
    }

    // Get inputs
    double* g=w; w += n_;
    casadi_copy(arg[QPSOL_G], n_, g);
    double* lbx=w; w += n_;
    casadi_copy(arg[QPSOL_LBX], n_, lbx);
    double* ubx=w; w += n_;
    casadi_copy(arg[QPSOL_UBX], n_, ubx);
    double* lba=w; w += nc_;
    casadi_copy(arg[QPSOL_LBA], nc_, lba);
    double* uba=w; w += nc_;
    casadi_copy(arg[QPSOL_UBA], nc_, uba);
    double* H=w; w += nnz_in(QPSOL_H);
    casadi_copy(arg[QPSOL_H], nnz_in(QPSOL_H), H);
    double* A=w; w += nnz_in(QPSOL_A);
    casadi_copy(arg[QPSOL_A], nnz_in(QPSOL_A), A);
    double* x=w; w += n_;
    casadi_copy(arg[QPSOL_X0], n_, x);
    double* lam_x=w; w += n_;
    casadi_copy(arg[QPSOL_LAM_X0], n_, lam_x);

    // Temporaries
    double* lam_a=w; w += nc_;

    int status;

    // We change method in crossover
    if (m.is_warm && qp_method_ == 7) {
      status = CPXsetintparam(m.env, CPX_PARAM_QPMETHOD, 1);
    }

    for (int i = 0; i < nc_; ++i) {
      // CPX_INFBOUND

      // Equality
      if (uba[i] - lba[i] < 1e-20) {
        m.sense[i] = 'E';
        m.rhs[i] = lba[i];
        m.rngval[i] = 0.;
      } else if (lba[i] < -CPX_INFBOUND) {
        // Ineq - no lower bound
        m.sense[i] = 'L';
        m.rhs[i] = uba[i];
        m.rngval[i] = 0.;
      } else if (uba[i] > CPX_INFBOUND) {
        // Ineq - no upper bound
        m.sense[i] = 'G';
        m.rhs[i] = lba[i];
        m.rngval[i] = 0.;
      } else { // Inew both upper and lower bounds
        m.sense[i] = 'R';
        m.rhs[i] = lba[i];
        m.rngval[i] = uba[i] - lba[i];
      }
    }

    // Copying objective, constraints, and bounds.
    const Sparsity& A_sp = sparsity_in(QPSOL_A);
    const int* matbeg = A_sp.colind();
    const int* matind = A_sp.row();
    const double* matval = A;
    const double* obj = g;
    const double* lb = lbx;
    const double* ub = ubx;
    status = CPXcopylp(m.env, m.lp, n_, nc_, m.objsen, obj, get_ptr(m.rhs), get_ptr(m.sense),
                       matbeg, get_ptr(m.matcnt), matind, matval, lb, ub, get_ptr(m.rngval));

    // Preparing coefficient matrix Q
    const Sparsity& H_sp = sparsity_in(QPSOL_H);
    const int* qmatbeg = H_sp.colind();
    const int* qmatind = H_sp.row();
    const double* qmatval = H;
    status = CPXcopyquad(m.env, m.lp, qmatbeg, get_ptr(m.qmatcnt), qmatind, qmatval);

    if (dump_to_file_) {
      CPXwriteprob(m.env, m.lp, dump_filename_.c_str(), "LP");
    }

    // Warm-starting if possible
    if (qp_method_ != 0 && qp_method_ != 4 && m.is_warm) {
      // TODO(Joel): Initialize slacks and dual variables of bound constraints
      CPXcopystart(m.env, m.lp, get_ptr(m.cstat), get_ptr(m.rstat), x, 0, 0, lam_x);
    } else {
      status = CPXcopystart(m.env, m.lp, 0, 0, x, 0, 0, lam_x);
    }

    // Optimize...
    status = CPXqpopt(m.env, m.lp);

    if (status) {
      casadi_error("CPLEX: Failed to solve QP...");
    }
    // Retrieving solution
    int solstat;

    double f;
    std::vector<double> slack;
    slack.resize(nc_);
    status = CPXsolution(m.env, m.lp, &solstat, &f, x, lam_a, get_ptr(slack), lam_x);
    if (status) {
      userOut() << "CPLEX: Failed to get solution.\n";
    }

    // Retrieving the basis
    if (qp_method_ != 0 && qp_method_ != 4) {
      status = CPXgetbase(m.env, m.lp, get_ptr(m.cstat), get_ptr(m.rstat));
    }

    // Flip the sign of the multipliers
    casadi_scal(nc_, -1., lam_a);
    casadi_scal(n_, -1., lam_x);

    int solnstat = CPXgetstat(m.env, m.lp);
    stringstream errormsg;
    // NOTE: Why not print directly to userOut() and userOut<true, PL_WARN>()?
    if (verbose()) {
      if (solnstat == CPX_STAT_OPTIMAL) {
        errormsg << "CPLEX: solution status: Optimal solution found.\n";
      } else if (solnstat == CPX_STAT_UNBOUNDED) {
        errormsg << "CPLEX: solution status: Model is unbounded\n";
      } else if (solnstat == CPX_STAT_INFEASIBLE) {
        errormsg << "CPLEX: solution status: Model is infeasible\n";
      } else if (solnstat == CPX_STAT_INForUNBD) {
        errormsg << "CPLEX: solution status: Model is infeasible or unbounded\n";
      } else if (solnstat == CPX_STAT_OPTIMAL_INFEAS) {
        errormsg << "CPLEX: solution status: Optimal solution "
            "is available but with infeasibilities\n";
      } else if (solnstat == CPX_STAT_NUM_BEST) {
        errormsg << "CPLEX: solution status: Solution available, but not "
            "proved optimal due to numeric difficulties.\n";
      } else if (solnstat == CPX_STAT_FIRSTORDER) {
        errormsg << "CPLEX: solution status: Solution satisfies first-order optimality "
            "conditions, but is not necessarily globally optimal.\n";
      } else {
        errormsg << "CPLEX: solution status: " <<  solnstat << "\n";
      }
      userOut() << errormsg.str();

      // Printing basis condition number
      //double cn;
      //status = CPXgetdblquality(m.env, m.lp, &cn, CPX_KAPPA);
      //userOut() << "CPLEX: Basis condition number: " << cn << endl;
    }
    if (solnstat != CPX_STAT_OPTIMAL) {
      //    throw CasadiException(errormsg.c_str());
    }

    // Next time we warm start
    if (warm_start_) {
      m.is_warm = true;
    }

    // Get the outputs
    if (res[QPSOL_COST]) *res[QPSOL_COST] = f;
    casadi_copy(lam_a, nc_, res[QPSOL_LAM_A]);
    casadi_copy(lam_x, n_, res[QPSOL_LAM_X]);
    casadi_copy(x, n_, res[QPSOL_X]);
  }

  CplexInterface::~CplexInterface() {
  }

  CplexMemory::CplexMemory() {
    // Setting warm-start flag
    this->is_warm = false;

    // Set pointer to zero to avoid deleting a nonexisting instance
    this->env = 0;
    this->lp = 0;
  }

  CplexMemory::~CplexMemory() {
    // Return flag
    int status;

    // Only free if Cplex problem if it has been allocated
    if (this->lp!=0) {
      status = CPXfreeprob(this->env, &this->lp);
      if (status!=0) {
        userOut<true, PL_WARN>() << "CPXfreeprob failed, error code " << status << ".\n";
      }
      this->lp = 0;
    }

    // Closing down license
    if (this->env!=0) {
      status = CPXcloseCPLEX(&this->env);
      if (status!=0) {
        userOut<true, PL_WARN>() << "CPXcloseCPLEX failed, error code " << status << ".\n";
      }
      this->env = 0;
    }
  }

} // end namespace casadi
