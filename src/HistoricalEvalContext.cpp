#include "jas/HistoricalEvalContext.h"

#include <algorithm>
#include <cassert>
#include <memory>

#include "jas/Exception.h"
#include "jas/FunctionModule.h"
#include "jas/Keywords.h"

#define __ctxtm(methodName) &HistoricalEvalContext::methodName

namespace jas {
using std::make_shared;
using std::move;

namespace func_name {
using namespace std::string_view_literals;
inline constexpr auto evchg = JASSTR("evchg");
inline constexpr auto snchg = JASSTR("snchg");
inline constexpr auto field = JASSTR("field");
inline constexpr auto field_cv = JASSTR("field_cv");
inline constexpr auto field_lv = JASSTR("field_lv");
inline constexpr auto hfield = JASSTR("hfield");
inline constexpr auto hfield2arr = JASSTR("hfield2arr");
inline constexpr auto last_eval = JASSTR("last_eval");
}  // namespace func_name

namespace cstr {
namespace {
static const auto path = JASSTR("path");
static const auto iid = JASSTR("iid");
static const auto snapshot = JASSTR("snapshot");
static const auto snlst = JASSTR("last");
static const auto sncur = JASSTR("cur");
static const auto h_field_cur = JASSTR("current_field__");
static const auto h_field_lst = JASSTR("last_field__");
static const auto path_sep = JASSTR("/");
}  // namespace
}  // namespace cstr

static HistoricalEvalContext::SnapshotIdx _toSnapshotIdx(
    const String& snapshot);
static Var _makeHistoricalData(Var newd, Var oldd = {});
static bool _hasHistoricalShape(const Var& data);

#define __mc_invokeOnParentIfNoData(func, defaultRet, ...) \
  if (!hasData()) {                                        \
    if (parent_) {                                         \
      return parent()->func(__VA_ARGS__);                  \
    } else {                                               \
      return defaultRet;                                   \
    }                                                      \
  }                                                        \
  (void*)(0)

const HistoricalEvalContext::CtxtFunctionsMap&
HistoricalEvalContext::funcsMap() {
  static const CtxtFunctionsMap funcs = {
      {func_name::snchg, __ctxtm(snchg)},
      {func_name::evchg, __ctxtm(evchg)},
      {func_name::field, __ctxtm(field)},
      {func_name::field_lv, __ctxtm(field_lv)},
      {func_name::field_cv, __ctxtm(field_cv)},
      {func_name::hfield, __ctxtm(hfield)},
      {func_name::hfield2arr, __ctxtm(hfield2arr)},
      {func_name::last_eval, __ctxtm(last_eval)},
  };
  return funcs;
}

HistoricalEvalContext::HistoricalEvalContext(HistoricalEvalContext* p,
                                             ContextArguments input, String id)
    : _Base(p, move(id), move(input)) {}

HistoricalEvalContext::HistoricalEvalContext(HistoricalEvalContext* p,
                                             Var currentSnapshot,
                                             Var lastSnapshot, String id)
    : _Base(p, move(id), {move(currentSnapshot), move(lastSnapshot)}) {}

HistoricalEvalContext::~HistoricalEvalContext() { syncEvalResult(); }

std::shared_ptr<HistoricalEvalContext> HistoricalEvalContext::make(
    HistoricalEvalContext* p, Var currentSnapshot, Var lastSnapshot,
    String id) {
  return make_shared<HistoricalEvalContext>(p, move(currentSnapshot),
                                            move(lastSnapshot), move(id));
}

bool HistoricalEvalContext::functionSupported(
    const StringView& functionName) const {
  if (funcsMap().find(functionName) != std::end(funcsMap())) {
    return true;
  } else {
    return _Base::functionSupported(functionName);
  }
}

Var HistoricalEvalContext::invoke(const String& funcName, const Var& param) {
  if (auto it = funcsMap().find(funcName); it != std::end(funcsMap())) {
    return (this->*(it->second))(param);
  } else {
    return _Base::invoke(funcName, param);
  }
}

Var HistoricalEvalContext::snapshotValue(const String& path,
                                         const String& snapshot) const {
  return snapshotValue(path, _toSnapshotIdx(snapshot));
}

Var HistoricalEvalContext::snapshotValue(const String& path,
                                         const SnapshotIdx snidx) const {
  if (snidx < args_.size()) {
    if (!path.empty()) {
      return args_[snidx].getPath(path);
    } else {
      return args_[snidx];
    }
  } else {
    return {};
  }
}

EvalContextPtr HistoricalEvalContext::subContext(const String& ctxtID,
                                                 ContextArguments input) {
  if ((input.size() == 1) && _hasHistoricalShape(input[0])) {
    return make(this, input[0].at(cstr::h_field_cur),
                input[0].at(cstr::h_field_lst), ctxtID);
  } else {
    return make_shared<HistoricalEvalContext>(this, input, ctxtID);
  }
}

bool HistoricalEvalContext::hasData() const {
  return std::any_of(std::begin(args_), std::end(args_),
                     [](auto& sn) { return !sn.isNull(); });
}

HistoricalEvalContext* HistoricalEvalContext::parent() const {
  if (parent_) {
    assert(typeid(*parent_) ==
           typeid(*const_cast<HistoricalEvalContext*>(this)));
    return static_cast<HistoricalEvalContext*>(parent_);
  }
  return nullptr;
}

String HistoricalEvalContext::contextPath() const {
  if (parent()) {
    return strJoin(parent()->contextPath(), cstr::path_sep, this->id_);
  } else {
    return this->id_;
  }
}

String HistoricalEvalContext::contextPath(const String& variableName) const {
  return strJoin(contextPath(), cstr::path_sep, variableName);
}

Var HistoricalEvalContext::snchg(const Var& jpath) {
  __mc_invokeOnParentIfNoData(snchg, false, jpath);
  auto path = jpath.getString();

  if (path.empty()) {
    return !args_.empty() && ((args_.size() == 1 /*has only new snapshot*/) ||
                              (args_[SnapshotIdxNew] != args_[SnapshotIdxOld]));
  } else {
    return snapshotValue(path, SnapshotIdxNew) !=
           snapshotValue(path, SnapshotIdxOld);
  }
}

Var HistoricalEvalContext::evchg(const Var& json) {
  __jas_func_throw_invalidargs_if(
      !json.isString(), "variable name must be non-empty string", json);

  auto variableName = json.asString();
  __jas_ni_func_throw_invalidargs_if(variableName.empty(),
                                     "variable name must not be empty");
  __jas_ni_func_throw_invalidargs_if(
      variableName.front() != TobeStoredVariablePrefix,
      strJoin("Only variable with prefix `", TobeStoredVariablePrefix,
              "` can be check evchg, given name: \"", variableName,
              "\" -> must be \"", TobeStoredVariablePrefix, variableName,
              "\""));
  Var changed = true;

  if (auto itVar = variables_.find(variableName);
      itVar != std::end(variables_)) {
    auto& levr = *lastEvalResult();
    changed = levr.getAt(contextPath(variableName)) != itVar->second;
  } else if (parent_) {
    changed = parent()->evchg(variableName);
  } else {
    changed = false;
  }
  return changed;
}

Var HistoricalEvalContext::field(const Var& params) {
  __mc_invokeOnParentIfNoData(field, Var{}, params);
  String path;
  String snapshot;
  if (params.isString()) {
    path = params.asString();
  } else if (params.isDict()) {
    path = params.getAt(cstr::path).getString();
    snapshot = params.getAt(cstr::snapshot).getString();
  }
  return snapshotValue(path, snapshot);
}

Var HistoricalEvalContext::field_lv(const Var& path) {
  __mc_invokeOnParentIfNoData(field_lv, Var{}, path);
  String sp;
  if (path.isString()) {
    sp = path.asString();
  } else {
    __jas_func_throw_invalidargs_if(!path.isNull(),
                                    "input must be string or nothing", path);
  }
  return snapshotValue(sp, SnapshotIdxOld);
}

Var HistoricalEvalContext::field_cv(const Var& path) {
  __mc_invokeOnParentIfNoData(field_cv, Var{}, path);
  String sp;
  if (path.isString()) {
    sp = path.asString();
  } else {
    __jas_func_throw_invalidargs_if(!path.isNull(), "must be string or nothing",
                                    path);
  }

  return snapshotValue(sp, SnapshotIdxNew);
}

Var HistoricalEvalContext::hfield2arr(const Var& params) {
  __jas_func_throw_invalidargs_if(!params.isList() || params.size() != 2,
                                  " input must be array of [path, iid]",
                                  params);
  return _hfield(params[0].getString(), params[1].getString());
}

Var HistoricalEvalContext::hfield(const Var& params) {
  String path;
  String iid;
  if (params.isString()) {
    path = params.asString();
  } else if (params.isDict()) {
    path = params.getAt(cstr::path).getString();
    iid = params.getAt(cstr::iid).getString();
  }
  return _hfield(path, iid);
}

Var HistoricalEvalContext::last_eval(const Var& jVarName) {
  __jas_func_throw_invalidargs_if(!jVarName.isString(),
                                  "Variable name must be string", jVarName);
  decltype(auto) variableName = jVarName.asString();
  __jas_ni_func_throw_invalidargs_if(variableName.empty(),
                                     "Variable name must not be empty");
  auto& lastEvals = *lastEvalResult();
  auto ctxtPath = contextPath(variableName);
  if (lastEvals.contains(ctxtPath)) {
    return lastEvals.getAt(ctxtPath);
  } else if (parent_) {
    return parent()->last_eval(jVarName);
  } else {
    return {};
  }
}

Var HistoricalEvalContext::_hfield(const String& path, const String& iid) {
  __mc_invokeOnParentIfNoData(_hfield, Var{}, path, iid);
  auto newd = snapshotValue(path);
  if (!newd.isList()) {
    return _makeHistoricalData(move(newd), snapshotValue(path, SnapshotIdxOld));
  }

  // is array:
  auto findItem = [&iid](const Var& list, const Var& itemIdVal) {
    Var out;
    for (auto& item : list.asList()) {
      if (item.contains(iid) && item.getAt(iid) == itemIdVal) {
        out = item;
        break;
      }
    }
    return out;
  };

  auto& newList = newd;
  auto oldList = snapshotValue(path, SnapshotIdxOld);

  if (!iid.empty()) {
    auto output = Var::list();
    for (auto& newItem : newList.asList()) {
      output.add(_makeHistoricalData(newItem, findItem(oldList, newItem[iid])));
    }
    return output;
  } else {
    auto oldListSize = oldList.isList() ? oldList.size() : 0;
    auto newListSize = newList.isList() ? newList.size() : 0;
    auto minSize = std::min(oldListSize, newListSize);
    auto output = Var::list();
    for (size_t i = 0; i < minSize; ++i) {
      output.add(_makeHistoricalData(newList[i], oldList[i]));
    }
    if (minSize == oldListSize) {
      for (size_t i = minSize; i < newListSize; ++i) {
        output.add(_makeHistoricalData(newList[i]));
      }
    } else {
      for (size_t i = minSize; i < oldListSize; ++i) {
        output.add(_makeHistoricalData({}, oldList[i]));
      }
    }
    return output;
  }
}

const HistoricalEvalContext::EvaluatedVariablesPtr&
HistoricalEvalContext::lastEvalResult() {
  if (!lastEvalResult_) {
    if (parent_) {
      lastEvalResult_ = parent()->lastEvalResult();
    } else {
      lastEvalResult_ =
          make_shared<EvaluatedVariables>(EvaluatedVariables::dict());
    }
  }
  return lastEvalResult_;
}

void HistoricalEvalContext::setLastEvalResult(
    HistoricalEvalContext::EvaluatedVariablesPtr res) {
  lastEvalResult_ = move(res);
}

void HistoricalEvalContext::syncEvalResult() {
  if (!variables_.empty()) {
    auto& evr = *lastEvalResult();
    auto thisCtxtPath = contextPath();
    for (auto& [var, val] : variables_) {
      if (!val.isNull() && var.front() == TobeStoredVariablePrefix) {
        evr[strJoin(thisCtxtPath, cstr::path_sep, var)] = val;
      }
    }
  }
}

bool HistoricalEvalContext::saveEvaluationResult(OStream& ostrm) {
  syncEvalResult();
  if (auto res = lastEvalResult()) {
    ostrm << JsonTrait::dump(res->toJson());
    return true;
  }
  return false;
}

bool HistoricalEvalContext::loadEvaluationResult(IStream& istrm) {
  auto json = JsonTrait::parse(istrm);
  if (JsonTrait::isObject(json)) {
    lastEvalResult_ = make_shared<EvaluatedVariables>(json);
    return true;
  }
  return false;
}

std::vector<String> HistoricalEvalContext::supportedFunctions() const {
  std::vector<String> funcs;
  for (auto& [key, val] : funcsMap()) {
    funcs.push_back(key);
  }
  return funcs;
}

static HistoricalEvalContext::SnapshotIdx _toSnapshotIdx(
    const String& snapshot) {
  if (snapshot.empty()) {
    return HistoricalEvalContext::SnapshotIdxNew;
  }

  else if (snapshot == cstr::snlst) {
    return HistoricalEvalContext::SnapshotIdxOld;
  } else if (snapshot == cstr::sncur) {
    return HistoricalEvalContext::SnapshotIdxNew;
  } else {
    __jas_ni_func_throw_invalidargs(strJoin("Snapshot can be empty or '",
                                            cstr::sncur, "' or '", cstr::snlst,
                                            "' only!"));
  }
  // never come here!
  return HistoricalEvalContext::SnapshotIdxNew;
}

static Var _makeHistoricalData(Var curd, Var lstd) {
  return Var::dict({
      {cstr::h_field_lst, move(lstd)},
      {cstr::h_field_cur, move(curd)},
  });
}

static bool _hasHistoricalShape(const Var& data) {
  return data.isDict() && data.size() == 2 &&
         data.contains(cstr::h_field_cur) && data.contains(cstr::h_field_lst);
}

}  // namespace jas
