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


#include "nlpsol_impl.hpp"
#include "casadi/core/timing.hpp"
#include <chrono>

using namespace std;
namespace casadi {

  bool has_nlpsol(const string& name) {
    return Nlpsol::hasPlugin(name);
  }

  void load_nlpsol(const string& name) {
    Nlpsol::loadPlugin(name);
  }

  string doc_nlpsol(const string& name) {
    return Nlpsol::getPlugin(name).doc;
  }

  Function nlpsol(const string& name, const string& solver,
                                const SXDict& nlp, const Dict& opts) {
    return nlpsol(name, solver, Nlpsol::map2problem(nlp), opts);
  }

  Function nlpsol(const string& name, const string& solver,
                                const MXDict& nlp, const Dict& opts) {
    return nlpsol(name, solver, Nlpsol::map2problem(nlp), opts);
  }

  Function nlpsol(const string& name, const string& solver,
                                const Function& nlp, const Dict& opts) {
    if (nlp.is_a("sxfunction")) {
      return nlpsol(name, solver, Nlpsol::fun2problem<SX>(nlp), opts);
    } else {
      return nlpsol(name, solver, Nlpsol::fun2problem<MX>(nlp), opts);
    }
  }

  Function nlpsol(const string& name, const string& solver,
                                const XProblem& nlp, const Dict& opts) {
    Function ret;
    ret.assignNode(Nlpsol::instantiatePlugin(name, solver, nlp));
    ret->construct(opts);
    return ret;
  }

  vector<string> nlpsol_in() {
    vector<string> ret(nlpsol_n_in());
    for (size_t i=0; i<ret.size(); ++i) ret[i]=nlpsol_in(i);
    return ret;
  }

  vector<string> nlpsol_out() {
    vector<string> ret(nlpsol_n_out());
    for (size_t i=0; i<ret.size(); ++i) ret[i]=nlpsol_out(i);
    return ret;
  }

  string nlpsol_in(int ind) {
    switch (static_cast<NlpsolInput>(ind)) {
    case NLPSOL_X0:     return "x0";
    case NLPSOL_P:      return "p";
    case NLPSOL_LBX:    return "lbx";
    case NLPSOL_UBX:    return "ubx";
    case NLPSOL_LBG:    return "lbg";
    case NLPSOL_UBG:    return "ubg";
    case NLPSOL_LAM_X0: return "lam_x0";
    case NLPSOL_LAM_G0: return "lam_g0";
    case NLPSOL_NUM_IN: break;
    }
    return string();
  }

  string nlpsol_out(int ind) {
    switch (static_cast<NlpsolOutput>(ind)) {
    case NLPSOL_X:     return "x";
    case NLPSOL_F:     return "f";
    case NLPSOL_G:     return "g";
    case NLPSOL_LAM_X: return "lam_x";
    case NLPSOL_LAM_G: return "lam_g";
    case NLPSOL_LAM_P: return "lam_p";
    case NLPSOL_NUM_OUT: break;
    }
    return string();
  }

  int nlpsol_n_in() {
    return NLPSOL_NUM_IN;
  }

  int nlpsol_n_out() {
    return NLPSOL_NUM_OUT;
  }

  Nlpsol::Nlpsol(const std::string& name, const XProblem& nlp)
    : FunctionInternal(name), nlp_(nlp) {

    // Set default options
    callback_step_ = 1;
    eval_errors_fatal_ = false;
    warn_initial_bounds_ = false;
    iteration_callback_ignore_errors_ = false;
  }

  Nlpsol::~Nlpsol() {
  }

  Sparsity Nlpsol::get_sparsity_in(int ind) const {
    switch (static_cast<NlpsolInput>(ind)) {
    case NLPSOL_X0:
    case NLPSOL_LBX:
    case NLPSOL_UBX:
    case NLPSOL_LAM_X0:
      return get_sparsity_out(NLPSOL_X);
    case NLPSOL_LBG:
    case NLPSOL_UBG:
    case NLPSOL_LAM_G0:
      return get_sparsity_out(NLPSOL_G);
    case NLPSOL_P:
      return nlp_.is_sx ? nlp_.sx_p->in[NL_P].sparsity()
        : nlp_.mx_p->in[NL_P].sparsity();
    case NLPSOL_NUM_IN: break;
    }
    return Sparsity();
  }

  Sparsity Nlpsol::get_sparsity_out(int ind) const {
    switch (static_cast<NlpsolOutput>(ind)) {
    case NLPSOL_F:
      return Sparsity::scalar();
    case NLPSOL_X:
    case NLPSOL_LAM_X:
      return nlp_.is_sx ? nlp_.sx_p->in[NL_X].sparsity()
        : nlp_.mx_p->in[NL_X].sparsity();
    case NLPSOL_LAM_G:
    case NLPSOL_G:
      return nlp_.is_sx ? nlp_.sx_p->out[NL_G].sparsity()
        : nlp_.mx_p->out[NL_G].sparsity();
    case NLPSOL_LAM_P:
      return get_sparsity_in(NLPSOL_P);
    case NLPSOL_NUM_OUT: break;
    }
    return Sparsity();
  }

  Options Nlpsol::options_
  = {{&FunctionInternal::options_},
     {{"expand",
       {OT_BOOL,
        "Expand the NLP function in terms of scalar operations, i.e. MX->SX"}},
      {"hess_lag",
       {OT_FUNCTION,
        "Function for calculating the Hessian of the Lagrangian (autogenerated by default)"}},
      {"hess_lag_options",
       {OT_DICT,
        "Options for the autogenerated Hessian of the Lagrangian."}},
      {"grad_lag",
       {OT_FUNCTION,
        "Function for calculating the gradient of the Lagrangian (autogenerated by default)"}},
      {"grad_lag_options",
       {OT_DICT,
        "Options for the autogenerated gradient of the Lagrangian."}},
      {"jac_g",
       {OT_FUNCTION,
        "Function for calculating the Jacobian of the constraints "
        "(autogenerated by default)"}},
      {"jac_g_options",
       {OT_DICT,
        "Options for the autogenerated Jacobian of the constraints."}},
      {"grad_f",
       {OT_FUNCTION,
        "Function for calculating the gradient of the objective "
        "(column, autogenerated by default)"}},
      {"grad_f_options",
       {OT_DICT,
        "Options for the autogenerated gradient of the objective."}},
      {"jac_f",
       {OT_FUNCTION,
        "Function for calculating the Jacobian of the objective "
        "(sparse row, autogenerated by default)"}},
      {"jac_f_options",
       {OT_DICT,
        "Options for the autogenerated Jacobian of the objective."}},
      {"iteration_callback",
       {OT_FUNCTION,
        "A function that will be called at each iteration with the solver as input. "
        "Check documentation of Callback."}},
      {"iteration_callback_step",
       {OT_INT,
        "Only call the callback function every few iterations."}},
      {"iteration_callback_ignore_errors",
       {OT_BOOL,
        "If set to true, errors thrown by iteration_callback will be ignored."}},
      {"ignore_check_vec",
       {OT_BOOL,
        "If set to true, the input shape of F will not be checked."}},
      {"warn_initial_bounds",
       {OT_BOOL,
        "Warn if the initial guess does not satisfy LBX and UBX"}},
      {"eval_errors_fatal",
       {OT_BOOL,
        "When errors occur during evaluation of f,g,...,"
        "stop the iterations"}},
      {"verbose_init",
       {OT_BOOL,
        "Print out timing information about "
        "the different stages of initialization"}}
     }
  };

  void Nlpsol::init(const Dict& opts) {
    // Call the initialization method of the base class
    FunctionInternal::init(opts);

    // Read options
    for (auto&& op : opts) {
      if (op.first=="iteration_callback") {
        fcallback_ = op.second;
      } else if (op.first=="iteration_callback_step") {
        callback_step_ = op.second;
      } else if (op.first=="eval_errors_fatal") {
        eval_errors_fatal_ = op.second;
      } else if (op.first=="warn_initial_bounds") {
        warn_initial_bounds_ = op.second;
      } else if (op.first=="iteration_callback_ignore_errors") {
        iteration_callback_ignore_errors_ = op.second;
      }
    }

    // Get dimensions
    nx_ = nnz_out(NLPSOL_X);
    np_ = nnz_in(NLPSOL_P);
    ng_ = nnz_out(NLPSOL_G);

    if (!fcallback_.is_null()) {
      // Consistency checks
      casadi_assert(!fcallback_.is_null());
      casadi_assert_message(fcallback_.n_out()==1 && fcallback_.numel_out()==1,
                            "Callback function must return a scalar");
      casadi_assert_message(fcallback_.n_in()==n_out(),
                            "Callback input signature must match the NLP solver output signature");
      for (int i=0; i<n_out(); ++i) {
        casadi_assert_message(fcallback_.size_in(i)==size_out(i),
                              "Callback function input size mismatch");
        // TODO(@jaeandersson): Wrap fcallback_ in a function with correct sparsity
        casadi_assert_message(fcallback_.sparsity_in(i)==sparsity_out(i),
                              "Not implemented");
      }

      // Allocate temporary memory
      alloc(fcallback_);
    }
  }

  void Nlpsol::init_memory(Memory& mem) const {
  }

  void Nlpsol::checkInputs(Memory& mem) const {
    NlpsolMemory& m = dynamic_cast<NlpsolMemory&>(mem);

    // Skip check?
    if (!inputs_check_) return;

    const double inf = std::numeric_limits<double>::infinity();

    // Detect ill-posed problems (simple bounds)
    for (int i=0; i<nx_; ++i) {
      double lbx = m.lbx ? m.lbx[i] : 0;
      double ubx = m.ubx ? m.ubx[i] : 0;
      double x0 = m.x0 ? m.x0[i] : 0;
      casadi_assert_message(!(lbx==inf || lbx>ubx || ubx==-inf),
                            "Ill-posed problem detected (x bounds)");
      if (warn_initial_bounds_ && (x0>ubx || x0<lbx)) {
        casadi_warning("Nlpsol: The initial guess does not satisfy LBX and UBX. "
                       "Option 'warn_initial_bounds' controls this warning.");
        break;
      }
    }

    // Detect ill-posed problems (nonlinear bounds)
    for (int i=0; i<ng_; ++i) {
      double lbg = m.lbg ? m.lbg[i] : 0;
      double ubg = m.ubg ? m.ubg[i] : 0;
      casadi_assert_message(!(lbg==inf || lbg>ubg || ubg==-inf),
                            "Ill-posed problem detected (g bounds)");
    }
  }

  std::map<std::string, Nlpsol::Plugin> Nlpsol::solvers_;

  const std::string Nlpsol::infix_ = "nlpsol";

  DM Nlpsol::getReducedHessian() {
    casadi_error("Nlpsol::getReducedHessian not defined for class "
                 << typeid(*this).name());
    return DM();
  }

  void Nlpsol::setOptionsFromFile(const std::string & file) {
    casadi_error("Nlpsol::setOptionsFromFile not defined for class "
                 << typeid(*this).name());
  }

  double Nlpsol::default_in(int ind) const {
    switch (ind) {
    case NLPSOL_LBX:
    case NLPSOL_LBG:
      return -std::numeric_limits<double>::infinity();
    case NLPSOL_UBX:
    case NLPSOL_UBG:
      return std::numeric_limits<double>::infinity();
    default:
      return 0;
    }
  }

  void Nlpsol::eval(Memory& mem, const double** arg, double** res, int* iw, double* w) const {
    // Reset the solver, prepare for solution
    setup(mem, arg, res, iw, w);

    // Solve the NLP
    solve(mem);
  }

  void Nlpsol::set_work(Memory& mem, const double**& arg, double**& res,
                        int*& iw, double*& w) const {
    NlpsolMemory& m = dynamic_cast<NlpsolMemory&>(mem);

    // Get input pointers
    m.x0 = arg[NLPSOL_X0];
    m.p = arg[NLPSOL_P];
    m.lbx = arg[NLPSOL_LBX];
    m.ubx = arg[NLPSOL_UBX];
    m.lbg = arg[NLPSOL_LBG];
    m.ubg = arg[NLPSOL_UBG];
    m.lam_x0 = arg[NLPSOL_LAM_X0];
    m.lam_g0 = arg[NLPSOL_LAM_G0];
    arg += NLPSOL_NUM_IN;

    // Get output pointers
    m.x = res[NLPSOL_X];
    m.f = res[NLPSOL_F];
    m.g = res[NLPSOL_G];
    m.lam_x = res[NLPSOL_LAM_X];
    m.lam_g = res[NLPSOL_LAM_G];
    m.lam_p = res[NLPSOL_LAM_P];
    res += NLPSOL_NUM_OUT;
  }

  void Nlpsol::set_temp(Memory& mem, const double** arg, double** res,
                        int* iw, double* w) const {
    NlpsolMemory& m = dynamic_cast<NlpsolMemory&>(mem);
    m.arg = arg;
    m.res = res;
    m.iw = iw;
    m.w = w;
  }

  int Nlpsol::calc_f(NlpsolMemory& m, const double* x, const double* p, double* f) const {
    // Respond to a possible Crl+C signals
    InterruptHandler::check();
    casadi_assert(f!=0);

    fill_n(m.arg, f_fcn_.n_in(), nullptr);
    m.arg[F_X] = x;
    m.arg[F_P] = p;
    fill_n(m.res, f_fcn_.n_out(), nullptr);
    m.res[F_F] = f;
    m.n_calc_f += 1;
    auto t_start = chrono::system_clock::now(); // start timer
    try {
      f_fcn_(m.arg, m.res, m.iw, m.w, 0);
    } catch(exception& ex) {
      // Fatal error
      userOut<true, PL_WARN>() << name() << ":calc_f failed:" << ex.what() << endl;
      return 1;
    }
    auto t_stop = chrono::system_clock::now(); // stop timer

    // Make sure not NaN or Inf
    if (!isfinite(*f)) {
      userOut<true, PL_WARN>() << name() << ":calc_f failed: Inf or NaN detected" << endl;
      return -1;
    }

    // Update stats
    m.n_calc_f += 1;
    m.t_calc_f += chrono::duration<double>(t_stop - t_start).count();

    // Success
    return 0;
  }

  int Nlpsol::calc_g(NlpsolMemory& m, const double* x, const double* p, double* g) const {
    // Respond to a possible Crl+C signals
    InterruptHandler::check();
    casadi_assert(g!=0);

    // Evaluate User function
    fill_n(m.arg, g_fcn_.n_in(), nullptr);
    m.arg[G_X] = x;
    m.arg[G_P] = p;
    fill_n(m.res, g_fcn_.n_out(), nullptr);
    m.res[G_G] = g;
    auto t_start = chrono::system_clock::now(); // start timer
    try {
      g_fcn_(m.arg, m.res, m.iw, m.w, 0);
    } catch(exception& ex) {
      // Fatal error
      userOut<true, PL_WARN>() << name() << ":calc_g failed:" << ex.what() << endl;
      return 1;
    }
    auto t_stop = chrono::system_clock::now(); // stop timer

    // Make sure not NaN or Inf
    if (!all_of(g, g+ng_, [](double v) { return isfinite(v);})) {
      userOut<true, PL_WARN>() << name() << ":calc_g failed: NaN or Inf detected" << endl;
      return -1;
    }

    // Update stats
    m.n_calc_g += 1;
    m.t_calc_g += chrono::duration<double>(t_stop - t_start).count();

    // Success
    return 0;
  }

  int Nlpsol::
  calc_fg(NlpsolMemory& m, const double* x, const double* p, double* f, double* g) const {
    fill_n(m.arg, fg_fcn_.n_in(), nullptr);
    m.arg[0] = x;
    m.arg[1] = p;
    fill_n(m.res, fg_fcn_.n_out(), nullptr);
    m.res[0] = f;
    m.res[1] = g;
    fg_fcn_(m.arg, m.res, m.iw, m.w, 0);

    // Success
    return 0;
  }

  int Nlpsol::
  calc_gf_jg(NlpsolMemory& m, const double* x, const double* p, double* gf, double* jg) const {
    fill_n(m.arg, gf_jg_fcn_.n_in(), nullptr);
    m.arg[0] = x;
    m.arg[1] = p;
    fill_n(m.res, gf_jg_fcn_.n_out(), nullptr);
    m.res[0] = gf;
    m.res[1] = jg;
    gf_jg_fcn_(m.arg, m.res, m.iw, m.w, 0);

    // Success
    return 0;
  }

  int Nlpsol::calc_grad_f(NlpsolMemory& m, const double* x,
                          const double* p, double* f, double* grad_f) const {
    // Respond to a possible Crl+C signals
    InterruptHandler::check();
    casadi_assert(grad_f!=0);

    fill_n(m.arg, grad_f_fcn_.n_in(), nullptr);
    m.arg[0] = x;
    m.arg[1] = p;
    fill_n(m.res, grad_f_fcn_.n_out(), nullptr);
    m.res[0] = f;
    m.res[1] = grad_f;
    grad_f_fcn_(m.arg, m.res, m.iw, m.w, 0);

    // Success
    return 0;
  }

  int Nlpsol::calc_jac_g(NlpsolMemory& m, const double* x,
                         const double* p, double* g, double* jac_g) const {
    // Respond to a possible Crl+C signals
    InterruptHandler::check();
    casadi_assert(jac_g!=0);

    // Evaluate User function
    fill_n(m.arg, jac_g_fcn_.n_in(), nullptr);
    m.arg[0] = x;
    m.arg[1] = p;
    fill_n(m.res, jac_g_fcn_.n_out(), nullptr);
    m.res[0] = g;
    m.res[1] = jac_g;
    jac_g_fcn_(m.arg, m.res, m.iw, m.w, 0);

    // Success
    return 0;
  }

  int Nlpsol::calc_jac_f(NlpsolMemory& m, const double* x,
                         const double* p, double* f, double* jac_f) const {
    // Respond to a possible Crl+C signals
    InterruptHandler::check();
    casadi_assert(jac_f!=0);

    // Evaluate User function
    fill_n(m.arg, jac_f_fcn_.n_in(), nullptr);
    m.arg[0] = x;
    m.arg[1] = p;
    fill_n(m.res, jac_f_fcn_.n_out(), nullptr);
    m.res[0] = f;
    m.res[1] = jac_f;
    jac_f_fcn_(m.arg, m.res, m.iw, m.w, 0);

    // Success
    return 0;
  }

  int Nlpsol::calc_hess_l(NlpsolMemory& m, const double* x, const double* p,
                          const double* sigma, const double* lambda,
                          double* hl) const {
    // Respond to a possible Crl+C signals
    InterruptHandler::check();

    // Evaluate User function
    fill_n(m.arg, hess_l_fcn_.n_in(), nullptr);
    m.arg[HL_X] = x;
    m.arg[HL_P] = p;
    m.arg[HL_LAM_F] = sigma;
    m.arg[HL_LAM_G] = lambda;
    fill_n(m.res, hess_l_fcn_.n_out(), nullptr);
    m.res[HL_HL] = hl;
    auto t_start = chrono::system_clock::now(); // start timer
    try {
      hess_l_fcn_(m.arg, m.res, m.iw, m.w, 0);
    } catch(exception& ex) {
      // Fatal error
      userOut<true, PL_WARN>() << name() << ":calc_hess_l failed:" << ex.what() << endl;
      return 1;
    }
    auto t_stop = chrono::system_clock::now(); // stop timer

    // Make sure not NaN or Inf
    if (!all_of(hl, hl+hesslag_sp_.nnz(), [](double v) { return isfinite(v);})) {
      userOut<true, PL_WARN>() << name() << ":calc_hess_l failed: NaN or Inf detected" << endl;
      return -1;
    }

    // Update stats
    m.n_calc_hess_l += 1;
    m.t_calc_hess_l += chrono::duration<double>(t_stop - t_start).count();

    // Success
    return 0;
  }

  template<typename M>
  void Nlpsol::_setup_f() {
    const Problem<M>& nlp = nlp_;
    std::vector<M> arg(F_NUM_IN);
    arg[F_X] = nlp.in[NL_X];
    arg[F_P] = nlp.in[NL_P];
    std::vector<M> res(F_NUM_OUT);
    res[F_F] = nlp.out[NL_F];
    f_fcn_ = Function("nlp_f", arg, res);
    alloc(f_fcn_);
  }

  void Nlpsol::setup_f() {
    if (nlp_.is_sx) {
      _setup_f<SX>();
    } else {
      _setup_f<MX>();
    }
  }

  template<typename M>
  void Nlpsol::_setup_g() {
    const Problem<M>& nlp = nlp_;
    std::vector<M> arg(G_NUM_IN);
    arg[G_X] = nlp.in[NL_X];
    arg[G_P] = nlp.in[NL_P];
    std::vector<M> res(G_NUM_OUT);
    res[G_G] = nlp.out[NL_G];
    g_fcn_ = Function("nlp_g", arg, res);
    alloc(g_fcn_);
  }

  void Nlpsol::setup_g() {
    if (nlp_.is_sx) {
      _setup_g<SX>();
    } else {
      _setup_g<MX>();
    }
  }

  template<typename M>
  void Nlpsol::_setup_fg() {
    const Problem<M>& nlp = nlp_;
    std::vector<M> arg = {nlp.in[NL_X], nlp.in[NL_P]};
    std::vector<M> res = {nlp.out[NL_F], nlp.out[NL_G]};
    fg_fcn_ = Function("nlp_fg", arg, res);
    alloc(fg_fcn_);
  }

  void Nlpsol::setup_fg() {
    if (nlp_.is_sx) {
      _setup_fg<SX>();
    } else {
      _setup_fg<MX>();
    }
  }

  template<typename M>
  void Nlpsol::_setup_gf_jg() {
    const Problem<M>& nlp = nlp_;
    std::vector<M> arg = {nlp.in[NL_X], nlp.in[NL_P]};
    std::vector<M> res = {M::gradient(nlp.out[NL_F], nlp.in[NL_X]),
                          M::jacobian(nlp.out[NL_G], nlp.in[NL_X])};
    gf_jg_fcn_ = Function("nlp_gf_jg", arg, res);
    jacg_sp_ = gf_jg_fcn_.sparsity_out(1);
    alloc(gf_jg_fcn_);
  }

  void Nlpsol::setup_gf_jg() {
    if (nlp_.is_sx) {
      _setup_gf_jg<SX>();
    } else {
      _setup_gf_jg<MX>();
    }
  }

  template<typename M>
  void Nlpsol::_setup_grad_f() {
    const Problem<M>& nlp = nlp_;
    M x = nlp.in[NL_X];
    M p = nlp.in[NL_P];
    M f = nlp.out[NL_F];
    M gf = M::gradient(f, x);
    gf = project(gf, x.sparsity());
    grad_f_fcn_ = Function("nlp_grad_f", {x, p}, {f, gf});
    alloc(grad_f_fcn_);
  }

  void Nlpsol::setup_grad_f() {
    if (nlp_.is_sx) {
      _setup_grad_f<SX>();
    } else {
      _setup_grad_f<MX>();
    }
  }

  template<typename M>
  void Nlpsol::_setup_jac_g() {
    const Problem<M>& nlp = nlp_;
    M x = nlp.in[NL_X];
    M p = nlp.in[NL_P];
    M f = nlp.out[NL_F];
    M g = nlp.out[NL_G];
    M J = M::jacobian(g, x);
    std::vector<M> arg = {x, p};
    std::vector<M> res = {g, J};
    jac_g_fcn_ = Function("nlp_jac_g", arg, res);
    jacg_sp_ = J.sparsity();
    alloc(jac_g_fcn_);
  }

  void Nlpsol::setup_jac_g() {
    if (nlp_.is_sx) {
      _setup_jac_g<SX>();
    } else {
      _setup_jac_g<MX>();
    }
  }

  template<typename M>
  void Nlpsol::_setup_jac_f() {
    const Problem<M>& nlp = nlp_;
    jac_f_fcn_ = Function("nlp_jac_f", nlp.in,
                          {nlp.out[NL_F], M::jacobian(nlp.out[NL_F], nlp.in[NL_X])});
    alloc(jac_f_fcn_);
  }

  void Nlpsol::setup_jac_f() {
    if (nlp_.is_sx) {
      _setup_jac_f<SX>();
    } else {
      _setup_jac_f<MX>();
    }
  }

  template<typename M>
  void Nlpsol::_setup_hess_l(bool tr, bool sym, bool diag) {
    const Problem<M>& nlp = nlp_;
    std::vector<M> arg(HL_NUM_IN);
    M x = arg[HL_X] = nlp.in[NL_X];
    arg[HL_P] = nlp.in[NL_P];
    M f = nlp.out[NL_F];
    M g = nlp.out[NL_G];
    M lam_f = arg[HL_LAM_F] = M::sym("lam_f", f.sparsity());
    M lam_g = arg[HL_LAM_G] = M::sym("lam_g", g.sparsity());
    std::vector<M> res(HL_NUM_OUT);
    res[HL_HL] = triu(M::hessian(dot(lam_f, f) + dot(lam_g, g), x));
    if (sym) res[HL_HL] = triu2symm(res[HL_HL]);
    if (tr) res[HL_HL] = res[HL_HL].T();
    hesslag_sp_ = res[HL_HL].sparsity();
    if (diag) {
      hesslag_sp_ = hesslag_sp_ + Sparsity::diag(hesslag_sp_.size1());
      res[HL_HL] = project(res[HL_HL], hesslag_sp_);
    }
    hess_l_fcn_ = Function("nlp_hess_l", arg, res);
    alloc(hess_l_fcn_);
  }

  void Nlpsol::setup_hess_l(bool tr, bool sym, bool diag) {
    if (nlp_.is_sx) {
      _setup_hess_l<SX>(tr, sym, diag);
    } else {
      _setup_hess_l<MX>(tr, sym, diag);
    }
  }

} // namespace casadi
