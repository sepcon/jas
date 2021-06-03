#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "CIF.h"
#include "ConsoleLogger.h"
#include "HistoricalEvalContext.h"
#include "Json.h"
#include "Keywords.h"
#include "Parser.h"
#include "SyntaxEvaluator.h"
#include "SyntaxValidator.h"
#include "Version.h"

using namespace std;
using namespace jas;
using namespace chrono_literals;
namespace fs = std::filesystem;
using chrono::system_clock;

static const CharType* JASE_TITLE = JASSTR("JAS - Json evAluation Syntax");
struct __CloggerSection {
  __CloggerSection(const String& sectionName) {
    __clogger << "==> " << sectionName << ":\n"
              << std::setfill(JASSTR('-')) << setw(sectionName.size() + 6)
              << "\n";
  }
  ~__CloggerSection() {
    __clogger << std::setfill(JASSTR('-')) << setw(30) << '\n';
  }
};

#include <mutex>
void showHelp(int exitCode = -1) {
  __CloggerSection showTitleSct{JASE_TITLE};
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
  __CloggerSection showTitleSct{JASE_TITLE};
  HistoricalEvalContext ctxt;
  auto functions = ctxt.supportedFunctions();
  auto displaySequence = [](const auto& sequence, const auto& prefix) {
    size_t maxLen = 0;
    int i = 0;
    for (auto& str : sequence) {
      if (str.size() > maxLen) {
        maxLen = str.size();
      }
    }
    for (auto& item : sequence) {
      ++i;
      __clogger << setiosflags(ios::left) << setw(maxLen + 2)
                << strJoin(prefix, item);
      if (i % 4 == 0) {
        __clogger << "\n";
      }
    }
  };

  if (!functions.empty()) {
    clogger() << "\nFunction keyword list:";
    displaySequence(functions, prefix::specifier);
  }

  auto& evbSpecifiers = parser::evaluableSpecifiers();
  if (!evbSpecifiers.empty()) {
    clogger() << "\nEvaluable keyword list:";
    displaySequence(evbSpecifiers, "");
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

#ifdef AXZ
int wmain
#else
int main
#endif
    (int argc, CharType** argv) {
  if (argc < 2) {
    showHelp();
  }

  checkInfoQueryInfoOption(argv[1]);
  // Parsing section...........

  __CloggerSection showTitleSct{JASE_TITLE};
  std::error_code ec;
  auto jasFile = fs::path{argv[1]};
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

//  try {
    auto historicalContext =
        make_shared<HistoricalEvalContext>(nullptr, jcurrentInput, jLastInput);
    auto evaluable = parser::parse(historicalContext, jexpression);

    clogger() << "JAS reconstructed: "
              << parser::reconstructJAS(historicalContext, jexpression);
    SyntaxEvaluator evaluator;

    SyntaxValidator validator;
    if (!validator.validate(evaluable)) {
      __CloggerSection syntaxErrorSct(JASSTR("Syntax Error"));
      clogger() << validator.getReport();
      exit(-1);
    } else {
      {
        validator.clear();
        __CloggerSection transformSyntaxSct(JASSTR("Transformed syntax"));
        clogger() << validator.generateSyntax(evaluable);
      }

      auto lastEvalResultFile = jasFile;
      lastEvalResultFile.replace_extension(".his");

      HistoricalEvalContext::EvaluationResultPtr lastEvalResult;
      if (fs::exists(lastEvalResultFile, ec)) {
        clogger() << "Load evaluation result from " << lastEvalResultFile;
        Ifstream lerifs{lastEvalResultFile};
        historicalContext->loadEvaluationResult(lerifs);
      }

      auto evalStart = system_clock::now();
      auto evaluated = evaluator.evaluate(evaluable, historicalContext);
      auto evalTime = chrono::duration_cast<chrono::microseconds>(
                          system_clock::now() - evalStart)
                          .count();

      {
        __CloggerSection evalResultSct(JASSTR("Evaluation result"));
        clogger() << evaluated;
        clogger() << "\nExc-time: (" << evalTime << ")ms!";
      }

      Ofstream ofs{lastEvalResultFile};
      if (historicalContext->saveEvaluationResult(ofs)) {
        clogger() << "Result saved to " << lastEvalResultFile;
      }
    }

//  } catch (const Exception& e) {
//    clogger() << "ERROR: " << e.what();
//    exit(-1);
//  }

  return 0;
}
