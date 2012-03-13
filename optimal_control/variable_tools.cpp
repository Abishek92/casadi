/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010 by Joel Andersson, Moritz Diehl, K.U.Leuven. All rights reserved.
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

#include "variable_tools.hpp"
using namespace std;

namespace CasADi{

vector<SX> var(const vector<Variable> v){
  vector<SX> ret(v.size());
  for(int i=0; i<v.size(); ++i)
    ret[i] = v[i].var();
  return ret;
}
    
vector<SX> der(const vector<Variable> v){
  vector<SX> ret(v.size());
  for(int i=0; i<v.size(); ++i)
    ret[i] = v[i].der();
  return ret;
}
    
vector<SX> highest(const vector<Variable> v){
  vector<SX> ret(v.size());
  for(int i=0; i<v.size(); ++i)
    ret[i] = v[i].highest();
  return ret;
}

vector<double> getNominal(const vector<Variable> v){
  vector<double> ret(v.size());
  for(int i=0; i<v.size(); ++i)
    ret[i] = v[i].getNominal();
  return ret;
}

std::vector<double> getAll(double (Variable::*fcn)() const, const std::vector<Variable> v, bool nominal){
  vector<double> ret(v.size());
  for(int i=0; i<v.size(); ++i)
    ret[i] = (v[i].*fcn)();

  if(nominal){
    vector<double> nominal = getNominal(v);
    for(int i=0; i<v.size(); ++i)
      ret[i] /= nominal[i];
  }

  return ret;
}


vector<double> getStart(const vector<Variable> v, bool nominal){
  return getAll(&Variable::getStart, v, nominal);
}

vector<double> getDerivativeStart(const vector<Variable> v, bool nominal){
  return getAll(&Variable::getDerivativeStart, v, nominal);
}

vector<double> getMin(const vector<Variable> v, bool nominal){
  return getAll(&Variable::getMin, v, nominal);
}

vector<double> getMax(const vector<Variable> v, bool nominal){
  return getAll(&Variable::getMax, v, nominal);
}

    
} // namespace CasADi
