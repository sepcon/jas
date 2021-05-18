#include "JEvalContext.h"

#include "Keywords.h"
#include "SupportedFunctions.h"

#define __CTXT_METHOD(methodName) __om(this, &JEvalContext::methodName)

namespace jas {

using JsonArray = typename JsonTrait::Array;
static void _setProperty(JEvalContext* ctxt, const String& name, DirectVal val);
static bool _isGloblProperty(const String& name);

JEvalContext::JEvalContext(JEvalContext* parent, Json new_snapshot,
                           Json old_snapshot)
    : parent_{parent},
      snapshots_{std::move(old_snapshot), std::move(new_snapshot)} {
  funcsmap_ = {
      {supported_func::snchg, __ni([this] { return snchg(); })},
      {supported_func::prop, __CTXT_METHOD(getProperty)},
      {supported_func::evchg, __CTXT_METHOD(evchg)},
  };
  if (parent_) {
    evalHistory_ = parent_->evalHistory();
  }
}

DirectVal JEvalContext::invokeFunction(const String& funcName,
                                       const DirectVal& param) {
  try {
    return globalInvoke(funcName, param);
  } catch (const function_not_found_error&) {
    return jas::invokeFunction(funcsmap_, funcName, param);
  }
}

DirectVal JEvalContext::snapshotValue(const String& path,
                                      Snapshot snapshot) const {
  if (snapshot < 2) {
    if (!path.empty()) {
      return DirectVal{JsonTrait::get(snapshots_[snapshot], path)};
    } else {
      return DirectVal{snapshots_[snapshot]};
    }
  }
  return {};
}

DirectVal JEvalContext::getProperty(const String& name) const {
  if (!JsonTrait::hasKey(properties_, name)) {
    throwIf<EvaluationError>(!parent_, "Ref value`", name, "` does not exist!");
    return parent_->getProperty(name);
  } else {
    return JsonTrait::get(properties_, name);
  }
}

void JEvalContext::setProperty(const String& name, DirectVal val) {
  if (_isGloblProperty(name)) {
    // This case for global property
    _setProperty(rootContext(), name, std::move(val));
  } else {
    _setProperty(this, name, std::move(val));
  }
}

EvalContexts JEvalContext::subContexts(const DirectVal& listSub,
                                       const String& itemId) {
  EvalContexts ctxts;
  if (!listSub.isType<JsonArray>()) {
    auto path = listSub.get<String>();
    auto jnewVal = JsonTrait::get(snapshots_[Snapshot::new_], path);
    auto joldVal = JsonTrait::get(snapshots_[Snapshot::old], path);
    auto new_list =
        path.empty()
            ? JsonTrait::template get<JsonArray>(snapshots_[Snapshot::new_])
            : JsonTrait::template get<JsonArray>(snapshots_[Snapshot::new_],
                                                 path);
    auto old_list =
        path.empty()
            ? JsonTrait::template get<JsonArray>(snapshots_[Snapshot::old])
            : JsonTrait::template get<JsonArray>(snapshots_[Snapshot::old],
                                                 path);
    auto find_item = [&itemId](const JsonArray& list, const Json& item_id_val) {
      for (auto& item : list) {
        if (JsonTrait::equal(JsonTrait::get(item, itemId), item_id_val)) {
          return item;
        }
      }
      return Json{};
    };

    if (!itemId.empty()) {
      for (auto& new_item : new_list) {
        auto old_item = find_item(old_list, JsonTrait::get(new_item, itemId));
        ctxts.emplace_back(new JEvalContext{this, new_item, old_item});
      }
    } else {
      auto old_list_size = JsonTrait::size(old_list);
      auto new_list_size = JsonTrait::size(new_list);
      JsonArray* longger_list = nullptr;
      JsonArray* shorter_list = nullptr;
      std::function<void(Json, Json)> add_ctxt;
      if (old_list_size < new_list_size) {
        shorter_list = &old_list;
        longger_list = &new_list;
        add_ctxt = [&ctxts, this](auto sitem, auto litem) {
          ctxts.emplace_back(
              new JEvalContext{this, std::move(litem), std::move(sitem)});
        };

      } else {
        shorter_list = &new_list;
        longger_list = &old_list;
        add_ctxt = [&ctxts, this](auto sitem, auto litem) {
          ctxts.emplace_back(
              new JEvalContext{this, std::move(sitem), std::move(litem)});
        };
      }

      auto siter = std::begin(*shorter_list);
      auto liter = std::begin(*longger_list);
      while (siter != std::end(*shorter_list)) {
        add_ctxt(*siter, *liter);
        ++siter;
        ++liter;
      }
      while (liter != std::end(*longger_list)) {
        add_ctxt(Json{}, *liter);
        ++liter;
      }
    }

  } else if (listSub.isType<JsonArray>()) {
    for (auto& item : listSub.get<JsonArray>()) {
      ctxts.push_back(std::make_shared<JEvalContext>(
          this, item, JsonTrait::get<String>(item, itemId)));
    }
  }
  return ctxts;
}

bool JEvalContext::snchg() const {
  return !JsonTrait::equal(snapshots_[Snapshot::new_],
                           snapshots_[Snapshot::old]);
}

bool JEvalContext::evchg(const DirectVal& evaluated) {
  throwIf<SyntaxError>(evaluated.id.empty(),
                       "Input of `evchg` function must have not-empty ID");
  auto evaluatedPath = strJoin(contextPath(), "/", evaluated.id);
  auto& valInHistory = (*evalHistory())[evaluatedPath];
  auto changed = !JsonTrait::equal(valInHistory, evaluated.value);
  valInHistory = evaluated.value;
  return changed;
}

JEvalContext* JEvalContext::rootContext() {
  auto root = this;
  while (root->parent_) {
    root = root->parent_;
  }
  return root;
}

String JEvalContext::contextPath() const {
  if (parent_) {
    return strJoin(parent_->contextPath(), "/", this->id_);
  } else {
    return this->id_;
  }
}

const JEvalContext::EvalHistoryPtr& JEvalContext::evalHistory() {
  if (!evalHistory_) {
    if (parent_) {
      evalHistory_ = parent_->evalHistory();
    } else {
      evalHistory_ = std::make_shared<EvalHistory>();
    }
  }
  return evalHistory_;
}

void JEvalContext::setEvalHistory(JEvalContext::EvalHistoryPtr history) {
  evalHistory_ = std::move(history);
}

static void _setProperty(JEvalContext* ctxt, const String& name,
                         DirectVal val) {
  ctxt->properties_[name] = std::move(val.value);
}

static bool _isGloblProperty(const String& name) {
  return name[1] == prefix::property;
}
}  // namespace jas
