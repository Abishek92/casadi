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


#include "function.hpp"

#include "sx_elem.hpp"
#include "matrix.hpp"
#include "sparse_storage.hpp"
#include "mx.hpp"

#include "casadi_limits.hpp"

#include "weak_ref.hpp"
#include <iostream>

#include "sparse_storage_impl.hpp"

using namespace std;
namespace casadi {

  template<class T>
  const T casadi_limits<T>::zero = T(0);

  template<class T>
  const T casadi_limits<T>::one = 1;

  template<class T>
  const T casadi_limits<T>::two = 2;

  template<class T>
  const T casadi_limits<T>::minus_one = -1;

  template class casadi_limits<double>;
  template class casadi_limits<int>;

  template class SparseStorage<Sparsity>;
  template class SparseStorage<WeakRef>;

} // namespace casadi

namespace std {
  #ifndef _MSC_VER
  template class std::numeric_limits<casadi::SXElem>;
  #endif

} // namespace std
