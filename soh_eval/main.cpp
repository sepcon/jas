#include <jas/ConsoleLogger.h>

#include <filesystem>
#include <fstream>

#include "SOHEvaluator.h"

using namespace std;

using namespace jas;
namespace fs = std::filesystem;
static Json readJsonFile(const fs::path& file) {
  using IStreamBufIterator = istreambuf_iterator<CharType>;
  using Ifstream = basic_ifstream<CharType>;

  Ifstream reader(file);
  if (reader) {
    return JsonTrait::parse(
        String(IStreamBufIterator{reader}, IStreamBufIterator{}));
  } else {
    return {};
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    clogerr() << "Please specify working dir ";
    return -1;
  }
  auto verbose = false;
  for (int i = 2; i < argc; ++i) {
    auto arg = strJoin(argv[i]);
    if (arg == JASSTR("--verbose")) {
      verbose = true;
      break;
    } else if (arg.find(JASSTR("--")) == 0) {
      clogerr() << "Unkown argument: " << arg;
      return -1;
    }
  }

  const fs::path sohDir = fs::u8path(argv[1]);
  auto sohNewReportFile = sohDir / "new_report.json";
  auto sohNewReport = readJsonFile(sohNewReportFile);
  auto evaluatoinGuide = readJsonFile(sohDir / "evaluation_guide.json");
  owc::SOHEvaluator ev(sohDir, evaluatoinGuide);
  if (JsonTrait::isNull(sohNewReport)) {
    clogerr() << "Please put new soh report to file: " << sohNewReportFile;
    return -1;
  }
  {
    CLoggerTimerSection evalutaingLogSct(JASSTR("SOH EVALUATION"));
    ev.setLogCallback([](const jas::String& str) { cloginfo() << str; });
    if (verbose) {
      ev.setDebugCallback([](const jas::String& str) { cloginfo() << str; });
    }
    ev.evaluate(sohNewReport);
    CloggerSection shouldReport(JASSTR("Should report"));
    cloginfo() << std::boolalpha << ev.shouldReport();
  }
  ev.saveEvalHistory();
  return 0;
}
