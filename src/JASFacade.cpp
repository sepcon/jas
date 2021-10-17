#include "jas/JASFacade.h"

#include "jas/BasicEvalContext.h"
#include "jas/ModuleManager.h"
#include "jas/SyntaxEvaluator.h"
#include "jas/SyntaxValidator.h"
#include "jas/Translator.h"

__module_creating_prototype(cif);
__module_creating_prototype(list);
__module_creating_prototype(dict);
__module_creating_prototype(alg);

namespace jas {

struct _JASFacade {
  _JASFacade() : parser(&moduleMgr) {
    addModule(mdl::cif::getModule());
    addModule(mdl::list::getModule());
    addModule(mdl::dict::getModule());
    addModule(mdl::alg::getModule());
  }
  _JASFacade(const Json &jasExpr, EvalContextPtr context) : _JASFacade() {
    this->context = move(context);
    setExpression(jasExpr);
  }

  void setExpression(const Json &expr) {
    evaluable = parser.translate(getContext(), expr);
  }

  EvalContextPtr getContext() {
    if (!context) {
      context = std::make_shared<BasicEvalContext>();
    }
    return context;
  }

  bool addModule(FunctionModulePtr module) {
    return moduleMgr.addModule(module->moduleName(), module);
  }

  ModuleManager moduleMgr;
  SyntaxEvaluator evaluator;
  Translator parser;
  EvaluablePtr evaluable;
  EvalContextPtr context;
};

JASFacade::JASFacade() : d_(new _JASFacade) {}

JASFacade::JASFacade(const Json &jasExpr, EvalContextPtr context)
    : d_(new _JASFacade(jasExpr, move(context))) {}

JASFacade::~JASFacade() {
  if (d_) {
    delete d_;
  }
}

Var JASFacade::evaluate(const Json &jasExpr, EvalContextPtr context) {
  d_->context = move(context);
  d_->setExpression(jasExpr);
  return evaluate();
}

Var JASFacade::evaluate() {
  assert(d_->evaluable);
  if (!d_->context) {
    d_->context = std::make_shared<BasicEvalContext>();
  }
  return d_->evaluator.evaluate(d_->evaluable, d_->context);
}

String JASFacade::getTransformedSyntax() noexcept {
  assert(d_->evaluable);
  return SyntaxValidator::syntaxOf(d_->evaluable);
}

void JASFacade::setContext(EvalContextPtr context) noexcept {
  d_->context = move(context);
}

void JASFacade::setExpression(const Json &jasExpr) {
  d_->setExpression(jasExpr);
}

bool JASFacade::addModule(FunctionModulePtr module) noexcept {
  return d_->addModule(move(module));
}

bool JASFacade::removeModule(const FunctionModulePtr &module) noexcept {
  return d_->moduleMgr.removeModule(module->moduleName());
}

bool JASFacade::removeModule(const String &moduleName) noexcept {
  return d_->moduleMgr.removeModule(moduleName);
}

FunctionNameList JASFacade::functionList(
    const EvalContextPtr &context) noexcept {
  auto lst = d_->moduleMgr.enumerateFuncions();
  lst.insert(std::end(lst), std::begin(d_->parser.evaluableSpecifiers()),
             std::end(d_->parser.evaluableSpecifiers()));
  if (context) {
    auto contextFuncs = context->supportedFunctions();
    lst.insert(std::end(lst), std::begin(contextFuncs), std::end(contextFuncs));
  }
  return lst;
}

SyntaxEvaluator *JASFacade::getEvaluator() noexcept {
  return std::addressof(d_->evaluator);
}

Translator *JASFacade::getParser() noexcept {
  return std::addressof(d_->parser);
}

ModuleManager *JASFacade::getModuleMgr() noexcept {
  return std::addressof(d_->moduleMgr);
}

void JASFacade::setDebugCallback(
    std::function<void(const String &)> callback) noexcept {
  d_->evaluator.setDebugInfoCallback(move(callback));
}

}  // namespace jas
