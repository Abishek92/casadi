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

#ifndef CASADI_MATLAB_EXPORT_HPP
#define CASADI_MATLAB_EXPORT_HPP

#include <string>
#include <iostream>
#include "casadi_misc.hpp"

namespace casadi {

#ifdef SWIG
class MatlabExport;
#endif // SWIG

#ifndef SWIG
class MatlabExport {
    public:
    MatlabExport(const std::string& filename);

    std::string save(const std::vector<double>& arg);
    std::string save(const std::vector<casadi_int>& arg, casadi_int offset=0);

    std::string load() const;

    std::string filename_int_;
    std::string filename_double_;
    std::ofstream stream_int_;
    std::ofstream stream_double_;

    casadi_int cnt_int_;
    casadi_int cnt_double_;
};

#endif // SWIG

} // namespace casadi

#endif // CASADI_MATLAB_EXPORT_HPP
