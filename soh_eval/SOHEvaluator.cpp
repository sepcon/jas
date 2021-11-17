#include "SOHEvaluator.h"

#include <jas/HistoricalEvalContext.h>
#include <jas/JASFacade.h>

#include <cassert>
#include <fstream>

namespace owc {
using std::make_shared;
using std::move;
namespace fs = std::filesystem;
using namespace jas;

template <typename... Msgs>
void evlog(const SOHEvaluator::LogCallback& logcb, const Msgs&... msgs) {
  if (logcb) {
    logcb(strJoin(msgs...));
  }
}

template <typename... Msgs>
void catEvlog(const SOHEvaluator::LogCallback& logcb, const String& category,
              const Msgs&... msgs) {
  evlog(logcb, "Evalutaing category [", category, "]: ", msgs...);
}

#define SOH_CAT_LOG(...) catEvlog(this->logCallback_, __VA_ARGS__)
#define SOH_LOG(...) evlog(this->logCallback_, __VA_ARGS__)

using Ofstream = std::basic_ofstream<CharType>;
using Ifstream = std::basic_ifstream<CharType>;

auto constexpr SOH_LAST_REPORT_FILE_NAME = "soh_last_report.json";
auto constexpr SOH_LAST_EVAL_FILE_NAME = "soh_last_eval.json";

SOHEvaluator::SOHEvaluator(FSPath workingDir, Json evaluationGuide)
    : workingDir_(move(workingDir)), evaluationGuide_(move(evaluationGuide)) {
  _loadEvalHistory();
}

bool SOHEvaluator::evaluate(Json report) {
  auto evalSucceeded = _evaluate(report);
  lastReport_ = move(report);
  if (!evalSucceeded) {
    lastEvals_.clear();
  }
  return evalSucceeded;
}

void SOHEvaluator::setLogCallback(LogCallback logcb) {
  assert(logcb);
  logCallback_ = move(logcb);
}

void SOHEvaluator::setDebugCallback(LogCallback logcb) {
  debugCallback_ = move(logcb);
}

bool SOHEvaluator::saveEvalHistory() const {
  auto ret = false;
  auto writeFile = [this](auto&& filename, auto&& jsonContent) {
    auto filepath = workingDir_ / filename;
    Ofstream writer(filepath);
    if (!writer) {
      SOH_LOG("Failed open file to write file ", filepath);
    } else {
      writer << jsonContent;
    }
  };
  do {
    std::error_code ec;

    if (fs::is_directory(workingDir_, ec)) {
      if (fs::create_directories(workingDir_, ec); ec) {
        SOH_LOG("Failed to create working dir at ", workingDir_);
        break;
      }
    }

    writeFile(SOH_LAST_EVAL_FILE_NAME, lastEvals_.dump());

    if (shouldReport_) {
      writeFile(SOH_LAST_REPORT_FILE_NAME, JsonTrait::dump(lastReport_));
    }
    ret = true;
  } while (false);
  return ret;
}

bool SOHEvaluator::_loadEvalHistory() {
  auto readJsonFile = [](Ifstream& ifs) {
    String content{std::istreambuf_iterator<CharType>{ifs},
                   std::istreambuf_iterator<CharType>{}};
    return JsonTrait::parse(content);
  };
  auto ret = false;
  SOH_LOG("Prepare loading evaluation history...");
  do {
    std::error_code ec;
    if (!fs::exists(workingDir_)) {
      SOH_LOG("Working dir does not exist");
      break;
    }
    Ifstream lastEvalReader(workingDir_ / SOH_LAST_EVAL_FILE_NAME);
    Ifstream lastReportReader(workingDir_ / SOH_LAST_REPORT_FILE_NAME);
    if (!lastEvalReader || !lastReportReader) {
      SOH_LOG("Failed to open history files in ", workingDir_);
      break;
    }

    lastEvals_ = readJsonFile(lastEvalReader);
    lastReport_ = readJsonFile(lastReportReader);
    if (!lastEvals_.isDict()) {
      SOH_LOG("Last eval result empty!");
    }
    if (JsonTrait::isNull(lastReport_)) {
      SOH_LOG("Last report is null");
    }
    ret = true;
  } while (false);
  return ret;
}

void SOHEvaluator::setWorkingDir(FSPath workingDir) {
  workingDir_ = move(workingDir);
}

bool SOHEvaluator::shouldReport() const { return shouldReport_; }

const SOHEvaluator::LastEvaluatedVariables& SOHEvaluator::lastEvalDetails()
    const {
  return lastEvals_;
}

bool SOHEvaluator::_evaluate(Json report) {
  if (!JsonTrait::isObject(evaluationGuide_)) {
    SOH_LOG("Evaluation guide is not json object: ",
            JsonTrait::dump(evaluationGuide_));
    return false;
  }

  _cleanUpEvalHistory();

  auto shouldReport = false;
  JASFacade jasFacade;
  jasFacade.setDebugCallback(debugCallback_);
  auto allSucceeded = JsonTrait::iterateObject(
      report, [&](const String& category, const Json& data) {
        if (!JsonTrait::hasKey(evaluationGuide_, category)) {
          SOH_CAT_LOG(
              category,
              "Does not exist corresponding category in evaluation guide");
          return false;
        }

        try {
          decltype(auto) evalExpr = JsonTrait::get(evaluationGuide_, category);
          auto context = make_shared<HistoricalEvalContext>(
              nullptr, data, JsonTrait::get(lastReport_, category));

          // last evaluation can be used by jas expr to evaluate current report
          context->setLastEvalResult(make_shared<Var>(lastEvals_[category]));

          SOH_CAT_LOG(category, "Start evaluating...");
          auto evaluatedVal = jasFacade.evaluate(evalExpr, context);
          if (evaluatedVal.isNull()) {
            SOH_CAT_LOG(category, "Evaluated to null");
            return false;
          }

          // Should report will be evaluated to key: `ShouldReport`
          auto shouldReportThisCat =
              evaluatedVal.getAt(JASSTR("ReportWhen")).getValue<bool>(true);
          SOH_CAT_LOG(category,
                      "Should report this category: ", shouldReportThisCat);

          shouldReport |= shouldReportThisCat;

          context->syncEvalResult();
          auto lastCatEvals = context->lastEvalResult();
          if (lastCatEvals) {
            lastEvals_[category] = *lastCatEvals;
          } else {
            SOH_CAT_LOG(category, "evaluation details is empty, don't save it");
          }
          // continue with next category
          return true;
        } catch (const jas::Exception& e) {
          SOH_CAT_LOG(category, "caught exception: ", e.what());
          return false;
        }
      });

  if (allSucceeded) {
    shouldReport_ = shouldReport;
  } else {
    shouldReport_ = true;
  }
  return allSucceeded;
}

void SOHEvaluator::_cleanUpEvalHistory() {
  std::vector<String> tobeClean;
  if (lastEvals_.isDict()) {
    for (auto& [category, data] : lastEvals_.asDict()) {
      if (!JsonTrait::hasKey(evaluationGuide_, category)) {
        tobeClean.push_back(category);
      }
    }

    for (auto& category : tobeClean) {
      SOH_LOG("Category [", category,
              "] is no longer existed in evaluation guide, then remove it from "
              "last evaluation result");
      lastEvals_.erase(category);
    }
  }
}

}  // namespace owc
