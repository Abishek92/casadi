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

#ifndef CASADI_CALLBACK_INTERNAL_HPP
#define CASADI_CALLBACK_INTERNAL_HPP

#include "callback.hpp"
#include "function_internal.hpp"

namespace casadi {

  class CASADI_EXPORT CallbackInternal : public FunctionInternal {
    friend class CallbackFunction;
  public:

    /** \brief Constructor */
    explicit CallbackInternal(Callback* self);

    /** \brief Destructor */
    virtual ~CallbackInternal();

    /** \brief  Evaluate numerically, work vectors given */
    virtual void evalD(const double** arg, double** res, int* iw, double* w);

    /** \brief  Initialize */
    virtual void init();

    /** \brief Finalize the object creation */
    virtual void finalize();

    ///@{
    /** \brief Return Jacobian of all input elements with respect to all output elements */
    virtual bool hasFullJacobian() const;
    virtual Function getFullJacobian(const std::string& name, const Dict& opts);
    ///@}

    ///@{
    /** \brief Return function that calculates forward derivatives */
    virtual Function getDerForward(const std::string& name, int nfwd, Dict& opts);
    virtual int numDerForward() const;
    ///@}

    ///@{
    /** \brief Return function that calculates adjoint derivatives */
    virtual Function getDerReverse(const std::string& name, int nadj, Dict& opts);
    virtual int numDerReverse() const;
    ///@}

    /** \brief Pointer to the public class */
    Callback* self_;

    /** \brief Is the public class owned by the internal class? */
    bool own_;
  };

} // namespace casadi

#endif // CASADI_CALLBACK_INTERNAL_HPP
