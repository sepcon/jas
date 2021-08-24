#include <chrono>
#include <filesystem>
#include <fstream>

#include "jas/ConsoleLogger.h"
#include "jas/HistoricalEvalContext.h"
#include "jas/JASFacade.h"
#include "jas/Json.h"
#include "jas/Keywords.h"
#include "jas/ModuleManager.h"
#include "jas/Translator.h"
#include "jas/Version.h"

using namespace std;
using namespace jas;
using namespace chrono_literals;
namespace fs = std::filesystem;
using chrono::system_clock;

static const CharType* JASE_TITLE = JASSTR("JAS - Json evAluation Syntax");
static JASFacade& jasFacade() {
  static JASFacade _;
  return _;
}

void showHelp(int exitCode = -1) {
  CloggerSection showTitleSct{JASE_TITLE};
  clogger() << JASSTR(
      R"(JASE - JAS Evaluator: Parse jas.dat file that contains jas syntax and data for evaluation and show the
evaluated result!

Usage1: jase.exe /path/to/jas.dat
    structure of jas.dat:
      - line1: jas syntax (json)
      - [line2]: (optional) current input data (json), historically - current snapshot
      - [line3]: (optional) last input data (json), historically - last snapshot

Usage2: jase.exe [--help|--keywords|--version]
    - help: show help instruction and exit
    - keywords: display available keywords
    - version: display current JAS version
)");

  exit(exitCode);
}

template <class... Msgs>
void exitIf(bool condition, const Msgs&... msg) {
  if (condition) {
    auto clg = clogger();
    clg << "ERROR: ";
    (clg << ... << msg);
    exit(-1);
  }
}

void showSupportedKeywords() {
  CloggerSection showTitleSct{JASE_TITLE};
  HistoricalEvalContext ctxt;
  auto displaySequence = [](const auto& sequence, const auto& prefix) {
    size_t maxLen = 0;
    const size_t MIN_COLUMN_WIDTH = 20;
    int i = 0;
    for (auto& str : sequence) {
      if (str.size() > maxLen) {
        maxLen = str.size();
      }
    }
    maxLen = maxLen < MIN_COLUMN_WIDTH ? MIN_COLUMN_WIDTH : maxLen;
    for (auto& item : sequence) {
      ++i;
      __clogger << setiosflags(ios::left) << setw(maxLen + 2)
                << strJoin(prefix, item);
      if (i % 4 == 0) {
        __clogger << "\n";
      }
    }
    // For odd list.count, we don't have last new line, then put one here
    if (i % 4 != 0) {
      __clogger << "\n";
    }
  };

  if (auto& evbSpecifiers = Translator::evaluableSpecifiers();
      !evbSpecifiers.empty()) {
    clogger() << "\nBuilt-in operators:";
    displaySequence(evbSpecifiers, "");
  }

  if (auto historicalContextFncs = ctxt.supportedFunctions();
      !historicalContextFncs.empty()) {
    clogger() << "\nHistoricalContext's Functions:";
    displaySequence(historicalContextFncs, prefix::specifier);
  }

  for (auto& [name, module] : jasFacade().getModuleMgr()->modules()) {
    clogger() << "\n"
              << (name.empty() ? JASSTR("Global")
                               : strJoin("Module `", name, "`"))
              << JASSTR(" functions:");
    FunctionNameList funcs;
    module->enumerateFuncs(funcs);
    displaySequence(funcs, prefix::specifier);
  }

  exit(0);
}

void checkInfoQueryInfoOption(const String& argv1) {
  if (argv1 == JASSTR("--keywords")) {
    showSupportedKeywords();
  } else if (argv1 == JASSTR("--help")) {
    showHelp(0);
  } else if (argv1 == JASSTR("--version")) {
    clogger() << jas::version;
    exit(0);
  } else if (argv1.size() > 2 && argv1.find(JASSTR("--")) != String::npos) {
    clogger() << "Unknown option: " << argv1;
    showHelp();
  }
}
using Ifstream = basic_ifstream<CharType>;
using Ofstream = basic_ofstream<CharType>;

int main(int argc, char** argv) {
  if (argc < 2) {
    showHelp();
  }

  OStringStream oss;
  oss << argv[1];
  checkInfoQueryInfoOption(oss.str());
  // Parsing section...........

  CLoggerTimerSection showTitleSct{JASE_TITLE};
  std::error_code ec;
  auto jasFile = fs::path{oss.str()};
  exitIf(!fs::exists(jasFile, ec), "Input file doens't exist!");

  Ifstream ifs(jasFile);
  exitIf(!ifs.is_open(), "Cannot open file ", jasFile);
  String strExpression, strCurrentInput, strLastInput;

  exitIf(!getline(ifs, strExpression), "Failed to read jas expression");
  auto jexpression = JsonTrait::parse(strExpression);
  if (getline(ifs, strCurrentInput)) {
    if (!getline(ifs, strLastInput)) {
      clogger() << "No last input data";
    }
  } else {
    clogger() << "No input data";
  }

  auto jcurrentInput = JsonTrait::parse(strCurrentInput);
  auto jLastInput = JsonTrait::parse(strLastInput);

  try {
    auto historicalContext =
        make_shared<HistoricalEvalContext>(nullptr, jcurrentInput, jLastInput);

    auto lastEvalResultFile = jasFile;
    lastEvalResultFile.replace_extension(".his");
    auto debugLogFile = jasFile;
    debugLogFile.replace_extension(".debug");

    HistoricalEvalContext::EvaluatedVariablesPtr lastEvalResult;
    if (fs::exists(lastEvalResultFile, ec)) {
      clogger() << "Load evaluation result from " << lastEvalResultFile;
      Ifstream lerifs{lastEvalResultFile};
      historicalContext->loadEvaluationResult(lerifs);
    }
    {
      clogger() << "JAS reconstructed: "
                << jasFacade()
                       .getParser()
                       ->reconstructJAS(historicalContext, jexpression)
                       .toJson();

      jasFacade().setContext(historicalContext);
      jasFacade().setExpression(jexpression);

      CloggerSection transformSyntaxSct(JASSTR("Transformed syntax"));
      clogger() << jasFacade().getTransformedSyntax();
    }
    {
      CLoggerTimerSection evalResultSct(JASSTR("Evaluation result"));
      Ofstream debugLogFileStream{debugLogFile};
      jasFacade().setDebugCallback([&debugLogFileStream](const auto& msg) {
        debugLogFileStream << msg << "\n";
      });

      auto evaluated = jasFacade().evaluate();
      clogger() << (evaluated.toJson());
    }

    Ofstream lastResultFileStream{lastEvalResultFile};
    if (historicalContext->saveEvaluationResult(lastResultFileStream)) {
      clogger() << "Result saved to " << lastEvalResultFile;
    }
  } catch (const Exception& e) {
    clogger() << "ERROR: " << e.what();
    exit(-1);
  }

  return 0;
}
