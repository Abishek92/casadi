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


#ifndef CASADI_GUROBI_INTERFACE_HPP
#define CASADI_GUROBI_INTERFACE_HPP

#include "casadi/core/function/qpsol_impl.hpp"
#include <casadi/interfaces/gurobi/casadi_qpsol_gurobi_export.h>

// GUROBI header
extern "C" {
#include "gurobi_c.h" // NOLINT(build/include)
}

/** \defgroup plugin_Qpsol_gurobi
    Interface to the GUROBI Solver for quadratic programming
*/

/** \pluginsection{Qpsol,gurobi} */

/// \cond INTERNAL
namespace casadi {

  struct CASADI_QPSOL_GUROBI_EXPORT GurobiMemory : public Memory {
    // Gurobi environment
    GRBenv *env;

    /// Constructor
    GurobiMemory();

    /// Destructor
    virtual ~GurobiMemory();
  };

  /** \brief \pluginbrief{Qpsol,gurobi}

      @copydoc Qpsol_doc
      @copydoc plugin_Qpsol_gurobi

  */
  class CASADI_QPSOL_GUROBI_EXPORT GurobiInterface : public Qpsol {
  public:
    /** \brief  Create a new Solver */
    explicit GurobiInterface(const std::string& name,
                             const std::map<std::string, Sparsity>& st);

    /** \brief  Create a new QP Solver */
    static Qpsol* creator(const std::string& name,
                                     const std::map<std::string, Sparsity>& st) {
      return new GurobiInterface(name, st);
    }

    /** \brief  Destructor */
    virtual ~GurobiInterface();

    // Get name of the plugin
    virtual const char* plugin_name() const { return "gurobi";}

    ///@{
    /** \brief Options */
    static Options options_;
    virtual const Options& get_options() const { return options_;}
    ///@}

    /** \brief  Initialize */
    virtual void init(const Dict& opts);

    /** \brief Create memory block */
    virtual Memory* memory() const { return new GurobiMemory();}

    /** \brief Initalize memory block */
    virtual void init_memory(Memory& mem) const;

    /// Solve the QP
    virtual void eval(Memory& mem, const double** arg, double** res, int* iw, double* w) const;

    /// Can discrete variables be treated
    virtual bool integer_support() const { return true;}

    /// A documentation string
    static const std::string meta_doc;

    // Variable types
    std::vector<char> vtype_;
  };

} // namespace casadi

/// \endcond
#endif // CASADI_GUROBI_INTERFACE_HPP

