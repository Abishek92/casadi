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


#include "csparse_cholesky_interface.hpp"

/// \cond INTERFACE

using namespace std;
namespace casadi {

  extern "C"
  int CASADI_LINSOL_CSPARSECHOLESKY_EXPORT
  casadi_register_linsol_csparsecholesky(Linsol::Plugin* plugin) {
    plugin->creator = CSparseCholeskyInterface::creator;
    plugin->name = "csparsecholesky";
    plugin->doc = CSparseCholeskyInterface::meta_doc.c_str();
    plugin->version = 30;
    return 0;
  }

  extern "C"
  void CASADI_LINSOL_CSPARSECHOLESKY_EXPORT casadi_load_linsol_csparsecholesky() {
    Linsol::registerPlugin(casadi_register_linsol_csparsecholesky);
  }

  CSparseCholeskyInterface::CSparseCholeskyInterface(const std::string& name,
                                                     const Sparsity& sparsity, int nrhs) :
    Linsol(name, sparsity, nrhs) {

    casadi_assert_message(sparsity.is_symmetric(),
                          "CSparseCholeskyInterface: supplied sparsity must be symmetric, got "
                          << sparsity.dim() << ".");
  }

  CSparseCholeskyInterface::~CSparseCholeskyInterface() {
  }

  CsparseCholMemory::~CsparseCholMemory() {
    if (this->S) cs_sfree(this->S);
    if (this->L) cs_nfree(this->L);
  }

  void CSparseCholeskyInterface::init(const Dict& opts) {
    // Call the init method of the base class
    Linsol::init(opts);
  }

  Memory* CSparseCholeskyInterface::memory() const {
    CsparseCholMemory* m = new CsparseCholMemory();
    try {
      m->L = 0;
      m->S = 0;
      m->A.nzmax = nnz_in(0);  // maximum number of entries
      m->A.sp = const_cast<int*>(static_cast<const int*>(sparsity_in(0)));
      m->A.x = 0; // numerical values, size nzmax

      // Temporary
      m->temp.resize(m->A.sp[1]);

      // ordering and symbolic analysis
      int order = 0; // ordering?
      m->S = static_cast<css*>(cs_calloc (1, sizeof (css)));
      int flag = cs_schol(m->S, order, &m->A);
      casadi_assert(flag==0);
      return m;
    } catch (...) {
      delete m;
      return 0;
    }
  }

  Sparsity CSparseCholeskyInterface::linsol_cholesky_sparsity(Memory& mem, bool tr) const {
    CsparseCholMemory& m = dynamic_cast<CsparseCholMemory&>(mem);

    casadi_assert(m.S);
    int n = m.A.sp[1];
    int nzmax = m.S->cp[n];
    std::vector< int > row(n+1);
    std::copy(m.S->cp, m.S->cp+n+1, row.begin());
    std::vector< int > colind(nzmax);
    int *Li = &colind.front();
    int *Lp = &row.front();
    cs* C;
    if (m.S->pinv) {
      C = static_cast<cs*>(cs_calloc(1, sizeof (cs)));
      cs_symperm(C, &m.A, m.S->pinv, 1);
    } else {
      C = &m.A;
    }
    std::vector< int > temp(2*n);
    int *c = &temp.front();
    int *s = c+n;
    for (int k = 0 ; k < n ; k++) c[k] = m.S->cp[k] ;
    for (int k = 0 ; k < n ; k++) {       /* compute L(k, :) for L*L' = C */
      int top = cs_ereach(C, k, m.S->parent, s, c) ;
      for ( ; top < n ; top++) {  /* solve L(0:k-1, 0:k-1) * x = C(:, k) */
          int i = s[top] ;               /* s[top..n-1] is pattern of L(k, :) */
          int p = c[i]++ ;
          Li[p] = k ;                /* store L(k, i) in row i */
      }
      int p = c[k]++ ;
      Li[p] = k ;
    }
    Lp[n] = m.S->cp[n] ;
    Sparsity ret(n, n, row, colind); // BUG?

    return tr ? ret.T() : ret;

  }

  DM CSparseCholeskyInterface::linsol_cholesky(Memory& mem, bool tr) const {
    CsparseCholMemory& m = dynamic_cast<CsparseCholMemory&>(mem);

    casadi_assert(m.L);
    cs *L = m.L->L;
    Sparsity sp = Sparsity::compressed(L->sp);
    std::vector< double > data(L->x, L->x + sp.nnz());
    DM ret(sp, data, false);
    return tr ? ret.T() : ret;
  }

  void CSparseCholeskyInterface::linsol_factorize(Memory& mem, const double* A) const {
    CsparseCholMemory& m = dynamic_cast<CsparseCholMemory&>(mem);

    // Set the nonzeros of the matrix
    casadi_assert(A!=0);
    m.A.x = const_cast<double*>(A);

    // Make sure that all entries of the linear system are valid
    int nnz = nnz_in(0);
    for (int k=0; k<nnz; ++k) {
      casadi_assert_message(!isnan(A[k]), "Nonzero " << k << " is not-a-number");
      casadi_assert_message(!isinf(A[k]), "Nonzero " << k << " is infinite");
    }

    if (m.L) cs_nfree(m.L);
    m.L = static_cast<csn*>(cs_calloc (1, sizeof (csn)));
    // numeric Cholesky factorization
    if (cs_chol(m.L, &m.A, m.S)) {
      casadi_error("Numeric Cholesky factorization failed");
    }
  }

  void CSparseCholeskyInterface::linsol_solve(Memory& mem, double* x, int nrhs, bool tr) const {
    CsparseCholMemory& m = dynamic_cast<CsparseCholMemory&>(mem);

    casadi_assert(m.L!=0);

    double *t = &m.temp.front();
    for (int k=0; k<nrhs; ++k) {
      if (tr) {
        cs_pvec(m.S->q, x, t, m.A.sp[1]) ;   // t = P1\b
        cs_ltsolve(m.L->L, t) ;               // t = L\t
        cs_lsolve(m.L->L, t) ;              // t = U\t
        cs_pvec(m.L->pinv, t, x, m.A.sp[1]) ;      // x = P2\t
      } else {
        cs_ipvec(m.L->pinv, x, t, m.A.sp[1]) ;   // t = P1\b
        cs_lsolve(m.L->L, t) ;               // t = L\t
        cs_ltsolve(m.L->L, t) ;              // t = U\t
        cs_ipvec(m.S->q, t, x, m.A.sp[1]) ;      // x = P2\t
      }
      x += ncol();
    }
  }

  void CSparseCholeskyInterface::linsol_solveL(Memory& mem, double* x, int nrhs, bool tr) const {
    CsparseCholMemory& m = dynamic_cast<CsparseCholMemory&>(mem);

    casadi_assert(m.L!=0);

    double *t = get_ptr(m.temp);

    for (int k=0; k<nrhs; ++k) {
      cs_ipvec(m.L->pinv, x, t, m.A.sp[1]) ;   // t = P1\b
      if (tr) cs_lsolve(m.L->L, t) ; // t = L\t
      if (!tr) cs_ltsolve(m.L->L, t) ; // t = U\t
      cs_ipvec(m.S->q, t, x, m.A.sp[1]) ;      // x = P2\t
      x += ncol();
    }
  }

} // namespace casadi

/// \endcond
