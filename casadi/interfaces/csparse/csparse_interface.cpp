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


#include "csparse_interface.hpp"
#include "casadi/core/global_options.hpp"

using namespace std;
namespace casadi {

  extern "C"
  int CASADI_LINSOL_CSPARSE_EXPORT
  casadi_register_linsol_csparse(Linsol::Plugin* plugin) {
    plugin->creator = CsparseInterface::creator;
    plugin->name = "csparse";
    plugin->doc = CsparseInterface::meta_doc.c_str();
    plugin->version = 30;
    return 0;
  }

  extern "C"
  void CASADI_LINSOL_CSPARSE_EXPORT casadi_load_linsol_csparse() {
    Linsol::registerPlugin(casadi_register_linsol_csparse);
  }

  CsparseInterface::CsparseInterface(const std::string& name,
                                     const Sparsity& sparsity, int nrhs)
    : Linsol(name, sparsity, nrhs) {
  }

  CsparseInterface::~CsparseInterface() {
  }

  CsparseMemory::~CsparseMemory() {
    if (this->S) cs_sfree(this->S);
    if (this->N) cs_nfree(this->N);
  }

  void CsparseInterface::init(const Dict& opts) {
    // Call the init method of the base class
    Linsol::init(opts);
  }

  Memory* CsparseInterface::memory() const {
    CsparseMemory* m = new CsparseMemory();
    try {
      m->N = 0;
      m->S = 0;
      m->A.nzmax = nnz_in(0);  // maximum number of entries
      m->A.sp = const_cast<int*>(static_cast<const int*>(sparsity_in(0)));
      m->A.i = const_cast<int*>(sparsity_in(0).row()); // row indices, size nzmax
      m->A.x = 0; // numerical values, size nzmax

      // Temporary
      m->temp_.resize(m->A.sp[1]);

      // Has the routine been called once
      m->called_once_ = false;

      return m;
    } catch (...) {
      delete m;
      return 0;
    }
  }

  void CsparseInterface::linsol_factorize(Memory& mem, const double* A) const {
    CsparseMemory& m = dynamic_cast<CsparseMemory&>(mem);
    casadi_assert(A!=0);

    // Set the nonzeros of the matrix
    m.A.x = const_cast<double*>(A);

    if (!m.called_once_) {
      if (verbose()) {
        userOut() << "CsparseInterface::prepare: symbolic factorization" << endl;
      }

      // ordering and symbolic analysis
      int order = 0; // ordering?
      if (m.S) cs_sfree(m.S);
      m.S = static_cast<css*>(cs_calloc(1, sizeof(css)));
      int flag = cs_sqr(m.S, order, &m.A, 0) ;
      casadi_assert(flag==0);
    }

    m.called_once_ = true;

    // Make sure that all entries of the linear system are valid
    for (int k=0; k<sparsity_.nnz(); ++k) {
      casadi_assert_message(!isnan(A[k]), "Nonzero " << k << " is not-a-number");
      casadi_assert_message(!isinf(A[k]), "Nonzero " << k << " is infinite");
    }

    if (verbose()) {
      userOut() << "CsparseInterface::prepare: numeric factorization" << endl;
      userOut() << "linear system to be factorized = " << endl;
      DM(sparsity_, vector<double>(A, A+sparsity_.nnz())).print_sparse();
    }

    // numeric LU factorization
    if (m.N) cs_nfree(m.N);
    m.N = static_cast<csn*>(cs_calloc(1, sizeof(csn)));
    double tol = 1e-8;
    if (cs_lu(m.N, &m.A, m.S, tol)) {
      DM temp(sparsity_, vector<double>(A, A+sparsity_.nnz()));
      temp = sparsify(temp);
      if (temp.sparsity().is_singular()) {
        stringstream ss;
        ss << "CsparseInterface::prepare: factorization failed due to matrix"
          " being singular. Matrix contains numerical zeros which are "
            "structurally non-zero. Promoting these zeros to be structural "
            "zeros, the matrix was found to be structurally rank deficient."
            " sprank: " << sprank(temp.sparsity()) << " <-> " << temp.size2() << endl;
        if (verbose()) {
          ss << "Sparsity of the linear system: " << endl;
          sparsity_.print(ss); // print detailed
        }
        throw CasadiException(ss.str());
      } else {
        stringstream ss;
        ss << "CsparseInterface::prepare: factorization failed, check if Jacobian is singular"
           << endl;
        if (verbose()) {
          ss << "Sparsity of the linear system: " << endl;
          sparsity_.print(ss); // print detailed
        }
        throw CasadiException(ss.str());
      }
    }
    casadi_assert(m.N!=0);
  }

  void CsparseInterface::linsol_solve(Memory& mem, double* x, int nrhs, bool tr) const {
    CsparseMemory& m = dynamic_cast<CsparseMemory&>(mem);
    casadi_assert(m.N!=0);

    double *t = &m.temp_.front();

    for (int k=0; k<nrhs; ++k) {
      if (tr) {
        cs_pvec(m.S->q, x, t, m.A.sp[1]) ;       // t = P2*b
        casadi_assert(m.N->U!=0);
        cs_utsolve(m.N->U, t) ;              // t = U'\t
        cs_ltsolve(m.N->L, t) ;              // t = L'\t
        cs_pvec(m.N->pinv, t, x, m.A.sp[1]) ;    // x = P1*t
      } else {
        cs_ipvec(m.N->pinv, x, t, m.A.sp[1]) ;   // t = P1\b
        cs_lsolve(m.N->L, t) ;               // t = L\t
        cs_usolve(m.N->U, t) ;               // t = U\t
        cs_ipvec(m.S->q, t, x, m.A.sp[1]) ;      // x = P2\t
      }
      x += ncol();
    }
  }

} // namespace casadi
