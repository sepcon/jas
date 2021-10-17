#include "jas/EvaluableClasses.h"
#include "jas/FunctionModuleBaseT.h"
#include "jas/SyntaxEvaluatorImpl.h"
#include "jas/SyntaxValidator.h"

namespace jas {
namespace mdl {
namespace alg {

#define __alg_func(name) \
  Var alg_##name(EvaluablePtr input, SyntaxEvaluatorImpl* evaluator)
#define __alg_mapping(name) \
  { JASSTR(#name), alg_##name }

#define __alg_try_begin try
#define __alg_try_end                           \
  catch (const jas::Exception& e) {             \
    __jas_ni_func_throw_invalidargs(e.details); \
  }                                             \
  catch (const std::exception& e) {             \
    __jas_ni_func_throw_invalidargs(e.what());  \
  }

#define __alg_makesure_params_is_list(input)                      \
  __jas_func_throw_invalidargs_if(!isType<EvaluableList>(input),  \
                                  " expect a list of parameters", \
                                  SyntaxValidator::syntaxOf(input))

#define __alg_makesure_param_count_eq(c, expectedCount) \
  __jas_ni_func_throw_invalidargs_if(                   \
      c.size() != expectedCount, " parameters must be [[list], condition]")

#define __alg_list_predicate_func_begin(FuncName, thelist, predicate) \
  __alg_func(FuncName) {                                              \
    __alg_makesure_params_is_list(input);                             \
    auto& args = _2args(input);                                       \
    __alg_makesure_param_count_eq(args, 2);                           \
                                                                      \
    auto varlist = evaluator->evalAndReturn(args.front().get());      \
                                                                      \
    __alg_try_begin {                                                 \
      varlist.detach();                                               \
      auto& thelist = varlist.asList();                               \
      auto& predicate = args.back();

#define __alg_list_predicate_func_end \
  }                                   \
  __alg_try_end;                      \
                                      \
  return {};                          \
  }

EvaluableList::value_type& _2args(const EvaluablePtr& input) {
  return static_cast<EvaluableList*>(input.get())->value;
}

__alg_list_predicate_func_begin(sort, tobesorted, predicate) {
  int idx = -1;
  std::sort(std::begin(tobesorted), std::end(tobesorted),
            [&](const Var& first, const Var& second) {
              return evaluator
                  ->evalAndReturn(predicate.get(), strJoin(++idx),
                                  {first, second})
                  .asBool();
            });
  return tobesorted;
}
__alg_list_predicate_func_end;

__alg_list_predicate_func_begin(filter, thelist, predicate) {
  auto filtered = Var::List{};
  for (auto& item : thelist) {
    if (evaluator->evalAndReturn(predicate.get(), {}, {item}).asBool()) {
      filtered.push_back(item);
    }
  }
  return filtered;
}
__alg_list_predicate_func_end;

__alg_list_predicate_func_begin(transform, thelist, predicate) {
  Var::List transformed;
  transformed.reserve(thelist.size());
  for (auto& item : thelist) {
    transformed.push_back(
        evaluator->evalAndReturn(predicate.get(), {}, {item}));
  }
  return transformed;
}
__alg_list_predicate_func_end;

__alg_list_predicate_func_begin(any_of, thelist, predicate) {
  using namespace std;
  return any_of(begin(thelist), end(thelist), [&](auto& item) {
    return evaluator->evalAndReturn(predicate.get(), {}, {item}).asBool();
  });
}
__alg_list_predicate_func_end;

__alg_list_predicate_func_begin(all_of, thelist, predicate) {
  using namespace std;
  return all_of(begin(thelist), end(thelist), [&](auto& item) {
    return evaluator->evalAndReturn(predicate.get(), {}, {item}).asBool();
  });
}
__alg_list_predicate_func_end;

__alg_list_predicate_func_begin(none_of, thelist, predicate) {
  using namespace std;
  return std::none_of(begin(thelist), end(thelist), [&](auto& item) {
    return evaluator->evalAndReturn(predicate.get(), {}, {item}).asBool();
  });
}
__alg_list_predicate_func_end;

__alg_list_predicate_func_begin(count_if, thelist, predicate) {
  size_t count = 0;
  for (auto& item : thelist) {
    if (evaluator->evalAndReturn(predicate.get(), {}, {item}).asBool()) {
      ++count;
    }
  }
  return count;
}
__alg_list_predicate_func_end;

using AlgorithmFunc = Var (*)(EvaluablePtr param,
                              SyntaxEvaluatorImpl* evaluator);

class AlgorithmModule : public FunctionModuleBaseT<AlgorithmFunc> {
 public:
  String moduleName() const override { return JASSTR("alg"); }
  const FunctionsMap& _funcMap() const override {
    static FunctionsMap _ = {
        __alg_mapping(sort),      __alg_mapping(filter),
        __alg_mapping(transform), __alg_mapping(any_of),
        __alg_mapping(all_of),    __alg_mapping(none_of),
        __alg_mapping(count_if),
    };
    return _;
  }

  Var invoke(const AlgorithmFunc& func, EvaluablePtr param,
             SyntaxEvaluatorImpl* evaluator) override {
    return func(param, evaluator);
  }
};

std::shared_ptr<FunctionModuleIF> getModule() {
  static auto module = std::make_shared<AlgorithmModule>();
  return module;
}

}  // namespace alg

}  // namespace mdl
}  // namespace jas
