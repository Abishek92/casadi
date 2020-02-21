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

#include "matlab_export.hpp"
#include <iostream>     // std::cout, std::fixed
#include <iomanip>      // std::setprecision

namespace casadi {

MatlabExport::MatlabExport(const std::string& filename) :
        filename_int_(filename+"_int.txt"), filename_double_(filename+"_double.txt"),
        stream_int_(filename_int_), stream_double_(filename_double_),
        cnt_int_(0), cnt_double_(0) {

        stream_double_ << std::setprecision(std::numeric_limits<double>::digits10 + 1) << std::scientific;
    }

    std::string MatlabExport::save(const std::vector<double>& arg) {
        for (double e : arg) {
            stream_double_ << e << "\n";
        }
        std::string ret = "doublevec(" + str(cnt_double_+1) + ":" + str(cnt_double_+arg.size()) + ")";
        cnt_double_+=arg.size();
        return ret;
    }

    std::string MatlabExport::save(const std::vector<casadi_int>& arg, casadi_int offset) {
        for (double e : arg) {
            stream_int_ << e+offset << "\n";
        }
        std::string ret = "intvec(" + str(cnt_int_+1) + ":" + str(cnt_int_+arg.size()) + ")";
        cnt_int_+=arg.size();
        return ret;
    }

    std::string MatlabExport::load() const {
        return "doublevec=load('" + filename_double_ + "','-ascii');intvec=load('" + filename_int_ + "','-ascii');";
    }

} // namespace casadi