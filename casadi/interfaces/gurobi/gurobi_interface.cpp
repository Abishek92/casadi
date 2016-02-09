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


#include "gurobi_interface.hpp"
#include "casadi/core/std_vector_tools.hpp"

using namespace std;
namespace casadi {

  extern "C"
  int CASADI_QPSOL_GUROBI_EXPORT
  casadi_register_qpsol_gurobi(Qpsol::Plugin* plugin) {
    plugin->creator = GurobiInterface::creator;
    plugin->name = "gurobi";
    plugin->doc = GurobiInterface::meta_doc.c_str();
    plugin->version = 30;
    return 0;
  }

  extern "C"
  void CASADI_QPSOL_GUROBI_EXPORT casadi_load_qpsol_gurobi() {
    Qpsol::registerPlugin(casadi_register_qpsol_gurobi);
  }

  GurobiInterface::GurobiInterface(const std::string& name,
                                   const std::map<std::string, Sparsity>& st)
    : Qpsol(name, st) {
  }

  GurobiInterface::~GurobiInterface() {
    clear_memory();
  }

  Options GurobiInterface::options_
  = {{&Qpsol::options_},
     {{"vtype",
       {OT_STRINGVECTOR,
        "Type of variables: [CONTINUOUS|binary|integer|semicont|semiint]"}}
     }
  };

  void GurobiInterface::init(const Dict& opts) {
    // Initialize the base classes
    Qpsol::init(opts);

    // Default options
    std::vector<std::string> vtype;

    // Read options
    for (auto&& op : opts) {
      if (op.first=="vtype") {
        vtype = op.second;
      }
    }

    // Variable types
    if (!vtype.empty()) {
      casadi_assert_message(vtype.size()==n_, "Option 'vtype' has wrong length");
      vtype_.resize(n_);
      for (int i=0; i<n_; ++i) {
        if (vtype[i]=="continuous") {
          vtype_[i] = GRB_CONTINUOUS;
        } else if (vtype[i]=="binary") {
          vtype_[i] = GRB_BINARY;
        } else if (vtype[i]=="integer") {
          vtype_[i] = GRB_INTEGER;
        } else if (vtype[i]=="semicont") {
          vtype_[i] = GRB_SEMICONT;
        } else if (vtype[i]=="semiint") {
          vtype_[i] = GRB_SEMIINT;
        } else {
          casadi_error("No such variable type: " + vtype[i]);
        }
      }
    }

    // Temporary memory
    alloc_w(n_, true); // val
    alloc_iw(n_, true); // ind
    alloc_iw(n_, true); // ind2
    alloc_iw(n_, true); // tr_ind
  }

  void GurobiInterface::init_memory(Memory* mem) const {
    auto m = static_cast<GurobiMemory*>(mem);

    // Load environment
    int flag = GRBloadenv(&m->env, 0); // no log file
    casadi_assert_message(!flag && m->env, "Failed to create GUROBI environment");
  }

  void GurobiInterface::
  eval(Memory* mem, const double** arg, double** res, int* iw, double* w) const {
    auto m = static_cast<GurobiMemory*>(mem);

    // Inputs
    const double *h=arg[QPSOL_H],
      *g=arg[QPSOL_G],
      *a=arg[QPSOL_A],
      *lba=arg[QPSOL_LBA],
      *uba=arg[QPSOL_UBA],
      *lbx=arg[QPSOL_LBX],
      *ubx=arg[QPSOL_UBX],
      *x0=arg[QPSOL_X0],
      *lam_x0=arg[QPSOL_LAM_X0];

    // Outputs
    double *x=res[QPSOL_X],
      *cost=res[QPSOL_COST],
      *lam_a=res[QPSOL_LAM_A],
      *lam_x=res[QPSOL_LAM_X];

    // Temporary memory
    double *val=w; w+=n_;
    int *ind=iw; iw+=n_;
    int *ind2=iw; iw+=n_;
    int *tr_ind=iw; iw+=n_;

    // Greate an empty model
    GRBmodel *model = 0;
    try {
      int flag = GRBnewmodel(m->env, &model, name_.c_str(), 0, 0, 0, 0, 0, 0);
      casadi_assert_message(!flag, GRBgeterrormsg(m->env));

      // Add variables
      for (int i=0; i<n_; ++i) {
        // Get bounds
        double lb = lbx ? lbx[i] : 0., ub = ubx ? ubx[i] : 0.;
        if (isinf(lb)) lb = -GRB_INFINITY;
        if (isinf(ub)) ub =  GRB_INFINITY;

        // Get variable type
        char vtype;
        if (!vtype_.empty()) {
          // Explicitly set 'vtype' takes precedence
          vtype = vtype_.at(i);
        } else if (!discrete_.empty() && discrete_.at(i)) {
          // Variable marked as discrete (integer or binary)
          vtype = lb==0 && ub==1 ? GRB_BINARY : GRB_INTEGER;
        } else {
          // Continious variable
          vtype = GRB_CONTINUOUS;
        }

        // Pass to model
        flag = GRBaddvar(model, 0, 0, 0, g ? g[i] : 0., lb, ub, vtype, 0);
        casadi_assert_message(!flag, GRBgeterrormsg(m->env));
      }
      flag = GRBupdatemodel(model);
      casadi_assert_message(!flag, GRBgeterrormsg(m->env));

      // Add quadratic terms
      const int *H_colind=sparsity_in(QPSOL_H).colind(), *H_row=sparsity_in(QPSOL_H).row();
      for (int i=0; i<n_; ++i) {

        // Quadratic term nonzero indices
        int numqnz = H_colind[1]-H_colind[0];
        casadi_copy(H_row, numqnz, ind);
        H_colind++;
        H_row += numqnz;

        // Corresponding column
        casadi_fill(ind2, numqnz, i);

        // Quadratic term nonzeros
        if (h) {
          casadi_copy(h, numqnz, val);
          casadi_scal(numqnz, 0.5, val);
          h += numqnz;
        } else {
          casadi_fill(val, numqnz, 0.);
        }

        // Pass to model
        flag = GRBaddqpterms(model, numqnz, ind, ind2, val);
        casadi_assert_message(!flag, GRBgeterrormsg(m->env));
      }

      // Add constraints
      const int *A_colind=sparsity_in(QPSOL_A).colind(), *A_row=sparsity_in(QPSOL_A).row();
      casadi_copy(A_colind, n_, tr_ind);
      for (int i=0; i<nc_; ++i) {
        // Get bounds
        double lb = lba ? lba[i] : 0., ub = uba ? uba[i] : 0.;
//        if (isinf(lb)) lb = -GRB_INFINITY;
//        if (isinf(ub)) ub =  GRB_INFINITY;

        // Constraint nonzeros
        int numnz = 0;
        for (int j=0; j<n_; ++j) {
          if (tr_ind[j]<A_colind[j+1] && A_row[tr_ind[j]]==i) {
            ind[numnz] = j;
            val[numnz] = a ? a[tr_ind[j]] : 0;
            numnz++;
            tr_ind[j]++;
          }
        }

        // Pass to model
        if (isinf(lb)) {
          if (isinf(ub)) {
            // Neither upper or lower bounds, skip
          } else {
            // Only upper bound
            flag = GRBaddconstr(model, numnz, ind, val, GRB_LESS_EQUAL, ub, 0);
            casadi_assert_message(!flag, GRBgeterrormsg(m->env));
          }
        } else {
          if (isinf(ub)) {
            // Only lower bound
            flag = GRBaddconstr(model, numnz, ind, val, GRB_GREATER_EQUAL, lb, 0);
            casadi_assert_message(!flag, GRBgeterrormsg(m->env));
          } else if (lb==ub) {
            // Upper and lower bounds equal
            flag = GRBaddconstr(model, numnz, ind, val, GRB_EQUAL, lb, 0);
            casadi_assert_message(!flag, GRBgeterrormsg(m->env));
          } else {
            // Both upper and lower bounds
            flag = GRBaddrangeconstr(model, numnz, ind, val, lb, ub, 0);
            casadi_assert_message(!flag, GRBgeterrormsg(m->env));
          }
        }
      }

      // Solve the optimization problem
      flag = GRBoptimize(model);
      casadi_assert_message(!flag, GRBgeterrormsg(m->env));
      int optimstatus;
      flag = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimstatus);
      casadi_assert_message(!flag, GRBgeterrormsg(m->env));

      // Get the objective value, if requested
      if (cost) {
        flag = GRBgetdblattr(model, GRB_DBL_ATTR_OBJVAL, cost);
        casadi_assert_message(!flag, GRBgeterrormsg(m->env));
      }

      // Get the optimal solution, if requested
      if (x) {
        flag = GRBgetdblattrarray(model, GRB_DBL_ATTR_X, 0, n_, x);
        casadi_assert_message(!flag, GRBgeterrormsg(m->env));
      }

      // Free memory
      GRBfreemodel(model);

    } catch (...) {
      // Free memory
      if (model) GRBfreemodel(model);
      throw;
    }
  }

  GurobiMemory::GurobiMemory() {
    this->env = 0;
  }

  GurobiMemory::~GurobiMemory() {
    if (this->env) GRBfreeenv(this->env);
  }

} // namespace casadi
