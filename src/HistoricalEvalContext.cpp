#include "jas/HistoricalEvalContext.h"

#include <algorithm>
#include <cassert>
#include <memory>

#include "jas/CIF.h"
#include "jas/Exception.h"
#include "jas/Keywords.h"

#define __ctxtm(methodName) &HistoricalEvalContext::methodName

namespace jas {
using std::make_shared;
using std::move;

namespace func_name {
using namespace std::string_view_literals;
inline constexpr auto prop = JASSTR("prop");
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
static JsonObject _makeHistoricalData(Json newd, Json oldd = {});
static bool _hasHistoricalShape(const Json& data);

#define __mc_invokeOnParentIfNoData(func, defaultRet, ...) \
  if (!hasData()) {                                        \
    if (parent_) {                                         \
      return parent()->func(__VA_ARGS__);                  \
    } else {                                               \
      return defaultRet;                                   \
    }                                                      \
  }                                                        \
  (void*)(0)

HistoricalEvalContext::CtxtFunctionsMap HistoricalEvalContext::funcsmap_ = {
    {func_name::snchg, __ctxtm(snchg)},
    {func_name::evchg, __ctxtm(evchg)},
    {func_name::field, __ctxtm(field)},
    {func_name::field_lv, __ctxtm(field_lv)},
    {func_name::field_cv, __ctxtm(field_cv)},
    {func_name::hfield, __ctxtm(hfield)},
    {func_name::hfield2arr, __ctxtm(hfield2arr)},
    {func_name::last_eval, __ctxtm(last_eval)},
};

HistoricalEvalContext::HistoricalEvalContext(HistoricalEvalContext* p,
                                             Json currentSnapshot,
                                             Json lastSnapshot, String id)
    : _Base(p, move(id)),
      snapshots_{move(lastSnapshot), move(currentSnapshot)} {}

HistoricalEvalContext::~HistoricalEvalContext() { syncEvalResult(); }

bool HistoricalEvalContext::functionSupported(
    const StringView& functionName) const {
  if (funcsmap_.find(functionName) != std::end(funcsmap_)) {
    return true;
  } else {
    return _Base::functionSupported(functionName);
  }
}

JsonAdapter HistoricalEvalContext::invoke(const String& funcName,
                                          const JsonAdapter& param) {
  if (auto it = funcsmap_.find(funcName); it != std::end(funcsmap_)) {
    return (this->*(it->second))(param);
  } else {
    return _Base::invoke(funcName, param);
  }
}

Json HistoricalEvalContext::snapshotValue(const String& path,
                                          const String& snapshot) const {
  return snapshotValue(path, _toSnapshotIdx(snapshot));
}

Json HistoricalEvalContext::snapshotValue(const String& path,
                                          const SnapshotIdx snidx) const {
  if (!path.empty()) {
    return JsonTrait::get(snapshots_[snidx], path);
  } else {
    return snapshots_[snidx];
  }
}

EvalContextPtr HistoricalEvalContext::subContext(const String& ctxtID,
                                                 const JsonAdapter& input) {
  if (_hasHistoricalShape(input)) {
    return make_shared<HistoricalEvalContext>(
        this, JsonTrait::get(input.value, cstr::h_field_cur),
        JsonTrait::get(input.value, cstr::h_field_lst), ctxtID);
  } else {
    return make_shared<HistoricalEvalContext>(this, input.value, Json{},
                                              ctxtID);
  }
}

bool HistoricalEvalContext::hasData() const {
  return std::any_of(std::begin(snapshots_), std::end(snapshots_),
                     [](auto& sn) { return !JsonTrait::isNull(sn); });
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

JsonAdapter HistoricalEvalContext::snchg(const Json& jpath) {
  __mc_invokeOnParentIfNoData(snchg, false, jpath);

  auto path = JsonTrait::get<String>(jpath);
  if (path.empty()) {
    return !JsonTrait::equal(snapshots_[SnapshotIdxNew],
                             snapshots_[SnapshotIdxOld]);
  } else {
    return !JsonTrait::equal(JsonTrait::get(snapshots_[SnapshotIdxNew], path),
                             JsonTrait::get(snapshots_[SnapshotIdxOld], path));
  }
}

JsonAdapter HistoricalEvalContext::evchg(const Json& json) {
  __jas_func_throw_invalidargs_if(!JsonTrait::isString(json),
                                  "property name must be non-empty string",
                                  json);

  auto propID = JsonTrait::get<String>(json);
  __jas_ni_func_throw_invalidargs_if(propID.empty(),
                                     "property name must not be empty");
  auto changed = true;

  if (auto itProp = properties_.find(propID); itProp != std::end(properties_)) {
    auto& levr = *lastEvalResult();
    changed = !JsonTrait::equal(JsonTrait::get(levr, contextPath(propID)),
                                *itProp->second);
  } else if (parent_) {
    changed = parent()->evchg(propID);
  } else {
    changed = false;
  }
  return changed;
}

JsonAdapter HistoricalEvalContext::field(const Json& params) {
  __mc_invokeOnParentIfNoData(field, {}, params);
  String path;
  String snapshot;
  if (JsonTrait::isString(params)) {
    path = JsonTrait::get<String>(params);
  } else if (JsonTrait::isObject(params)) {
    path = JsonTrait::get<String>(params, cstr::path);
    snapshot = JsonTrait::get<String>(params, cstr::snapshot);
  }
  return snapshotValue(path, snapshot);
}

JsonAdapter HistoricalEvalContext::field_lv(const Json& path) {
  __mc_invokeOnParentIfNoData(field_lv, {}, path);
  String sp;
  if (JsonTrait::isString(path)) {
    sp = JsonTrait::get<String>(path);
  } else {
    __jas_func_throw_invalidargs_if(!JsonTrait::isNull(path),
                                    "input must be string or nothing", path);
  }
  return snapshotValue(sp, SnapshotIdxOld);
}

JsonAdapter HistoricalEvalContext::field_cv(const Json& path) {
  __mc_invokeOnParentIfNoData(field_cv, {}, path);
  String sp;
  if (JsonTrait::isString(path)) {
    sp = JsonTrait::get<String>(path);
  } else {
    __jas_func_throw_invalidargs_if(JsonTrait::isNull(path),
                                    "must be string or nothing", path);
  }

  return snapshotValue(sp, SnapshotIdxNew);
}

JsonAdapter HistoricalEvalContext::hfield2arr(const Json& params) {
  __jas_func_throw_invalidargs_if(JsonTrait::size(params) != 2,
                                  " input must be array of [path, iid]",
                                  params);

  return hfield(
      JsonObject{{JASSTR("path"), params[0]}, {JASSTR("iid"), params[1]}});
}

JsonAdapter HistoricalEvalContext::hfield(const Json& params) {
  __mc_invokeOnParentIfNoData(hfield, {}, params);

  String path;
  String iid;

  if (JsonTrait::isString(params)) {
    path = JsonTrait::get<String>(params);
  } else if (JsonTrait::isObject(params)) {
    path = JsonTrait::get<String>(params, cstr::path);
    iid = JsonTrait::get<String>(params, cstr::iid);
  }
  auto newd = snapshotValue(path);
  if (!JsonTrait::isArray(newd)) {
    return _makeHistoricalData(move(newd), snapshotValue(path, SnapshotIdxOld));
  }

  // is array:
  auto findItem = [&iid](const Json& list, const Json& itemIdVal) {
    Json out;
    JsonTrait::iterateArray(list, [&](auto&& item) {
      if (JsonTrait::equal(JsonTrait::get(item, iid), itemIdVal)) {
        out = item;
        return false;
      }
      return true;
    });
    return out;
  };

  auto& newList = newd;
  auto oldList = snapshotValue(path, SnapshotIdxOld);

  if (!iid.empty()) {
    auto output = JsonTrait::array();
    JsonTrait::iterateArray(newList, [&](auto&& newItem) {
      JsonTrait::add(
          output,
          _makeHistoricalData(newItem,
                              findItem(oldList, JsonTrait::get(newItem, iid))));
      return true;
    });
    return output;
  } else {
    auto oldListSize = JsonTrait::size(newList);
    auto newListSize = JsonTrait::size(oldList);
    auto minSize = std::min(oldListSize, newListSize);
    auto output = JsonTrait::array();

    for (size_t i = 0; i < minSize; ++i) {
      JsonTrait::add(output, _makeHistoricalData(JsonTrait::get(newList, i),
                                                 JsonTrait::get(oldList, i)));
    }
    if (minSize == oldListSize) {
      for (size_t i = minSize; i < newListSize; ++i) {
        JsonTrait::add(output, _makeHistoricalData(JsonTrait::get(newList, i)));
      }
    } else {
      for (size_t i = minSize; i < oldListSize; ++i) {
        JsonTrait::add(output,
                       _makeHistoricalData({}, JsonTrait::get(oldList, i)));
      }
    }
    return output;
  }
}

JsonAdapter HistoricalEvalContext::last_eval(const Json& jVarName) {
  __jas_func_throw_invalidargs_if(!JsonTrait::isString(jVarName),
                                  "Variable name must be string", jVarName);
  decltype(auto) variableName = JsonTrait::get<String>(jVarName);
  __jas_ni_func_throw_invalidargs_if(variableName.empty(),
                                     "Variable name must not be empty");
  auto& lastEvals = *lastEvalResult();
  auto ctxtPath = contextPath(variableName);
  if (JsonTrait::hasKey(lastEvals, ctxtPath)) {
    return JsonTrait::get(lastEvals, ctxtPath);
  } else if (parent_) {
    return parent()->last_eval(jVarName);
  } else {
    return {};
  }
}

const HistoricalEvalContext::EvaluationResultPtr&
HistoricalEvalContext::lastEvalResult() {
  if (!lastEvalResult_) {
    if (parent_) {
      lastEvalResult_ = parent()->lastEvalResult();
    } else {
      lastEvalResult_ = make_shared<EvaluationResult>();
    }
  }
  return lastEvalResult_;
}

void HistoricalEvalContext::setLastEvalResult(
    HistoricalEvalContext::EvaluationResultPtr res) {
  lastEvalResult_ = move(res);
}

void HistoricalEvalContext::syncEvalResult() {
  if (!properties_.empty()) {
    auto& evr = *lastEvalResult();
    auto thisCtxtPath = contextPath();
    for (auto& [prop, val] : properties_) {
      if (val) {
        evr[strJoin(thisCtxtPath, cstr::path_sep, prop)] = val->value;
      }
    }
  }
}

bool HistoricalEvalContext::saveEvaluationResult(OStream& ostrm) {
  syncEvalResult();
  if (auto res = lastEvalResult()) {
    ostrm << JsonTrait::dump(*res);
    return true;
  }
  return false;
}

bool HistoricalEvalContext::loadEvaluationResult(IStream& istrm) {
  auto json = JsonTrait::parse(istrm);
  if (JsonTrait::isObject(json)) {
    lastEvalResult_ =
        make_shared<EvaluationResult>(JsonTrait::get<JsonObject>(json));
    return true;
  }
  return false;
}

std::vector<String> HistoricalEvalContext::supportedFunctions() const {
  std::vector<String> funcs;
  std::transform(begin(funcsmap_), std::end(funcsmap_),
                 std::back_insert_iterator(funcs),
                 [](auto&& p) { return p.first; });
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

static JsonObject _makeHistoricalData(Json curd, Json lstd) {
  return JsonTrait::object({
      {cstr::h_field_lst, move(lstd)},
      {cstr::h_field_cur, move(curd)},
  });
}

static bool _hasHistoricalShape(const Json& data) {
  return JsonTrait::isObject(data) && JsonTrait::size(data) == 2 &&
         JsonTrait::hasKey(data, cstr::h_field_cur) &&
         JsonTrait::hasKey(data, cstr::h_field_lst);
}
}  // namespace jas
