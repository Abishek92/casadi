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


#ifndef CASADI_QPOASES_INTERFACE_HPP
#define CASADI_QPOASES_INTERFACE_HPP

#include "casadi/core/function/qpsol_impl.hpp"
#include <casadi/interfaces/qpoases/casadi_qpsol_qpoases_export.h>
#include <qpOASES.hpp>

/** \defgroup plugin_Qpsol_qpoases
Interface to QPOases Solver for quadratic programming

*/

/** \pluginsection{Qpsol,qpoases} */

/// \cond INTERNAL
namespace casadi {

  struct CASADI_QPSOL_QPOASES_EXPORT QpoasesMemory : public Memory {
    /// QP Solver
    union {
      qpOASES::SQProblem *sqp;
      qpOASES::QProblemB *qp;
    };

    // Sparse QP matrices
    qpOASES::SymSparseMat *h;
    qpOASES::SparseMatrix *a;

    /// Has qpOASES been called once?
    bool called_once;

    /// Constructor
    QpoasesMemory();

    /// Destructor
    virtual ~QpoasesMemory();
  };

  /** \brief \pluginbrief{Qpsol,qpoases}
   *
   * @copydoc QPSolver_doc
   * @copydoc plugin_Qpsol_qpoases
   *
   * \author Joris Gillis, Joel Andersson
   * \date 2011
   *
   * */
  class CASADI_QPSOL_QPOASES_EXPORT QpoasesInterface : public Qpsol {
  public:
    /** \brief  Constructor */
    explicit QpoasesInterface();

    /** \brief  Create a new QP Solver */
    static Qpsol* creator(const std::string& name,
                          const std::map<std::string, Sparsity>& st) {
      return new QpoasesInterface(name, st);
    }

    /** \brief  Create a new Solver */
    explicit QpoasesInterface(const std::string& name,
                              const std::map<std::string, Sparsity>& st);

    /** \brief  Destructor */
    virtual ~QpoasesInterface();

    // Get name of the plugin
    virtual const char* plugin_name() const { return "qpoases";}

    ///@{
    /** \brief Options */
    static Options options_;
    virtual const Options& get_options() const { return options_;}
    ///@}

    /** \brief  Initialize */
    virtual void init(const Dict& opts);

    /** \brief Create memory block */
    virtual void* alloc_memory() const { return new QpoasesMemory();}

    /** \brief Free memory block */
    virtual void free_memory(void *mem) const { delete static_cast<QpoasesMemory*>(mem);}

    /** \brief Initalize memory block */
    virtual void init_memory(void* mem) const;

    /** \brief  Evaluate numerically */
    virtual void eval(void* mem, const double** arg, double** res, int* iw, double* w) const;

    /// A documentation string
    static const std::string meta_doc;

  protected:

    ///@{
    /// Convert between qpOASES types and standard types
    static bool from_BooleanType(qpOASES::BooleanType b);
    static qpOASES::BooleanType to_BooleanType(bool b);
    static std::string from_SubjectToStatus(qpOASES::SubjectToStatus b);
    static qpOASES::SubjectToStatus to_SubjectToStatus(std::string b);
    static std::string from_PrintLevel(qpOASES::PrintLevel b);
    static qpOASES::PrintLevel to_PrintLevel(std::string b);
    ///@}

    ///@{
    /// Options
    int max_nWSR_;
    double max_cputime_;
    qpOASES::Options ops_;
    bool sparse_;
    ///@}

    /// Throw error
    static void qpoases_error(const std::string& module, int flag);

    /// Get qpOASES error message
    static std::string getErrorMessage(int flag);

  };

} // namespace casadi

/// \endcond
#endif // CASADI_QPOASES_INTERFACE_HPP
