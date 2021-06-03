#include "HistoricalEvalContext.h"

#include <cassert>
#include <memory>

#include "Exception.h"
#include "Keywords.h"

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
static JsonObject _makeHistoryData(Json newd, Json oldd = {});
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
};

HistoricalEvalContext::HistoricalEvalContext(HistoricalEvalContext* p,
                                             Json currentSnapshot,
                                             Json lastSnapshot, String id)
    : _Base(p, move(id)),
      snapshots_{move(lastSnapshot), move(currentSnapshot)} {}

HistoricalEvalContext::~HistoricalEvalContext() { syncEvalResult(); }

bool HistoricalEvalContext::functionSupported(
    const String& functionName) const {
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
  auto propID = JsonTrait::get<String>(json);
  throwIf<SyntaxError>(
      propID.empty(),
      "Input of `evchg` function must be name of a property and not empty");
  auto changed = true;
  if (auto itProp = properties_.find(propID); itProp != std::end(properties_)) {
    auto& levr = *lastEvalResult();
    if (auto lastIt = levr.find(strJoin(contextPath(), cstr::path_sep, propID));
        lastIt != std::end(levr)) {
      changed = !JsonTrait::equal(lastIt->second, itProp->second);
    }
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
    throwIf<SyntaxError>(!JsonTrait::isNull(path), JASSTR("path of func `"),
                         func_name::field_lv,
                         JASSTR("` must be string or nothing, given is: "),
                         JsonTrait::dump(path));
  }
  return snapshotValue(sp, SnapshotIdxOld);
}

JsonAdapter HistoricalEvalContext::field_cv(const Json& path) {
  __mc_invokeOnParentIfNoData(field_cv, {}, path);
  String sp;
  if (JsonTrait::isString(path)) {
    sp = JsonTrait::get<String>(path);
  } else {
    throwIf<SyntaxError>(JsonTrait::isNull(path), JASSTR("path of func `"),
                         func_name::field_cv,
                         JASSTR("` must be string or nothing, given is: "),
                         JsonTrait::dump(path));
  }

  return snapshotValue(sp, SnapshotIdxNew);
}

JsonAdapter HistoricalEvalContext::hfield2arr(const Json& params) {
  throwIf<SyntaxError>(JsonTrait::size(params) != 2, JASSTR("param of `"),
                       func_name::hfield2arr,
                       JASSTR("` Must not be array of [path, iid]"));

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
    return _makeHistoryData(move(newd), snapshotValue(path, SnapshotIdxOld));
  }

  // is array:
  auto findItem = [&iid](const JsonArray& list, const Json& itemIdVal) {
    for (auto& item : list) {
      if (JsonTrait::equal(JsonTrait::get(item, iid), itemIdVal)) {
        return item;
      }
    }
    return Json{};
  };

  auto newList = JsonTrait::get<JsonArray>(newd);
  auto oldList = JsonTrait::get<JsonArray>(snapshotValue(path, SnapshotIdxOld));

  if (!iid.empty()) {
    JsonArray output;
    for (auto& newItem : newList) {
      JsonTrait::add(
          output,
          _makeHistoryData(move(newItem),
                           findItem(oldList, JsonTrait::get(newItem, iid))));
    }
    return output;
  } else {
    auto oldListSize = JsonTrait::size(oldList);
    auto newListSize = JsonTrait::size(newList);
    auto minSize = std::min(oldListSize, newListSize);
    JsonArray output;

    for (size_t i = 0; i < minSize; ++i) {
      JsonTrait::add(output, _makeHistoryData(newList[i], oldList[i]));
    }
    if (minSize == oldListSize) {
      for (size_t i = minSize; i < newListSize; ++i) {
        JsonTrait::add(output, _makeHistoryData(newList[i]));
      }
    } else {
      for (size_t i = minSize; i < oldListSize; ++i) {
        JsonTrait::add(output, _makeHistoryData({}, oldList[i]));
      }
    }
    return output;
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
  auto& evr = *lastEvalResult();
  auto thisCtxtPath = contextPath();
  for (auto& [prop, val] : properties_) {
    evr[strJoin(thisCtxtPath, cstr::path_sep, prop)] = val;
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
  String scontent{std::istreambuf_iterator<CharType>{istrm},
                  std::istreambuf_iterator<CharType>{}};
  if (!scontent.empty()) {
    auto json = JsonTrait::parse(scontent);
    if (JsonTrait::isObject(json)) {
      lastEvalResult_ =
          make_shared<EvaluationResult>(JsonTrait::get<JsonObject>(json));
      return true;
    }
  }
  return false;
}

std::vector<String> HistoricalEvalContext::supportedFunctions() const {
  auto funcs = _Base::supportedFunctions();
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
    throw_<SyntaxError>("Snapshot can be empty or '", cstr::sncur, "' or '",
                        cstr::snlst, "' only!");
  }
  // never come here!
  return HistoricalEvalContext::SnapshotIdxNew;
}

static JsonObject _makeHistoryData(Json curd, Json lstd) {
  return JsonObject{
      {cstr::h_field_lst, move(lstd)},
      {cstr::h_field_cur, move(curd)},
  };
}

static bool _hasHistoricalShape(const Json& data) {
  return JsonTrait::isType<JsonObject>(data) && JsonTrait::size(data) == 2 &&
         JsonTrait::hasKey(data, cstr::h_field_cur) &&
         JsonTrait::hasKey(data, cstr::h_field_lst);
}
}  // namespace jas
