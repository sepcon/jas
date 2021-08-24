
#include "jas/JASFacade.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "jas/ConsoleLogger.h"
#include "jas/HistoricalEvalContext.h"
#include "jas/JASFacade.h"
#include "jas/Json.h"
#include "jas/Translator.h"

namespace jas {
namespace fs = std::filesystem;

struct test_case {
  Json expected;
  Json rule;
  Json context_data;
  int data_line_number = -1;
};

using Ifstream = std::basic_ifstream<CharType>;
using test_cases = std::vector<test_case>;
__mc_jas_exception(data_error);

static JASFacade& jas_facade();
static int run_all_tests(const fs::path& testcase_dir);
static test_cases load_no_input_test_cases(const fs::path& data_file);
static test_cases load_has_input_test_cases(const fs::path& data_file);
static void run_test_case(const test_case& tc);
static void failed_test_case(const test_case& tc, const String& syntax,
                             const Var& observed, const String& reason = {});
static void success_test_case(const test_case& tc);
static Json parse_test_case(const String& data);
static EvalContextPtr make_eval_ctxt(Json data);

static int total_passes = 0;
static int total_failed = 0;

JASFacade& jas_facade() {
  static JASFacade _;
  return _;
}

static int run_all_tests(const fs::path& testcase_dir) {
  CLoggerTimerSection allTestSection(JASSTR("All test"));
  std::error_code ec;
  for (auto it = fs::directory_iterator{testcase_dir, ec};
       it != fs::directory_iterator{}; ++it) {
    if (it->is_regular_file(ec)) {
      auto test_data_file = it->path();
      test_cases (*load_test_cases)(const fs::path& data_file) = nullptr;
      if (test_data_file.extension() == ".ni") {
        load_test_cases = load_no_input_test_cases;
      } else if (test_data_file.extension() == ".hi") {
        load_test_cases = load_has_input_test_cases;
      }
      if (load_test_cases) {
        CLoggerTimerSection testFileSection{
            strJoin("Testing file: ", test_data_file)};
        try {
          auto tcs = load_test_cases(test_data_file);
          for (auto& tc : tcs) {
            try {
              run_test_case(tc);
            } catch (const data_error& e) {
              failed_test_case(tc, {}, {}, e.what());
            }
          }
        } catch (const data_error& de) {
          clogger() << "Failed to parse test case: " << de.what();
        }
      }
    }
  }
  clogger() << "\nSUMARY:"
            << "\nTotal passes: " << total_passes
            << "\nTotal failed: " << total_failed;
  return total_failed;
}

static test_cases load_no_input_test_cases(const fs::path& data_file) {
  Ifstream ifs{data_file};
  String line;
  int i = 0;
  int line_number = 0;
  Json rule;
  Json expected;

  test_cases tcs;
  while (std::getline(ifs, line)) {
    ++line_number;
    if (line.find(JASSTR("//")) == 0) {
      continue;
    }
    if (i == 0) {
      rule = parse_test_case(line);
    } else {
      expected = parse_test_case(line);
      tcs.push_back(test_case{std::move(expected), std::move(rule), nullptr,
                              line_number - 1});
    }
    i = (i + 1) % 2;
  }
  return tcs;
}

static test_cases load_has_input_test_cases(const fs::path& data_file) {
  Ifstream ifs{data_file};
  String line;
  int i = 0;
  int line_number = 0;
  Json rule;
  Json input;
  Json expected;

  test_cases tcs;
  while (std::getline(ifs, line)) {
    ++line_number;
    if (line.find(JASSTR("//")) == 0) {
      continue;
    }
    if (i == 0) {
      rule = parse_test_case(line);
    } else if (i == 1) {
      input = parse_test_case(line);
    } else {
      expected = parse_test_case(line);
      tcs.push_back(test_case{std::move(expected), std::move(rule),
                              std::move(input), line_number - 2});
    }
    i = (i + 1) % 3;
  }
  return tcs;
}
static void run_test_case(const test_case& tc) {
  try {
    auto evaluated =
        jas_facade().evaluate(tc.rule, make_eval_ctxt(tc.context_data));
    if (JsonTrait::equal(tc.expected, evaluated.toJson())) {
      success_test_case(tc);
    } else {
      failed_test_case(tc, jas_facade().getTransformedSyntax(), evaluated);
    }
  } catch (const Exception& e) {
    if (auto exceptionName =
            JsonTrait::get<String>(tc.expected, JASSTR("@exception"));
        !exceptionName.empty()) {
      if (e.details.find(exceptionName) != String::npos) {
        success_test_case(tc);
      }
    } else {
      failed_test_case(tc, JsonTrait::dump(tc.rule), {}, e.what());
    }
  }
}

static void failed_test_case(const test_case& tc, const String& syntax,
                             const Var& observed, const String& reason) {
  ++total_failed;
  clogger() << JASSTR("TC[") << tc.data_line_number
            << JASSTR("][FAILED] - syntax: ") << syntax;
  if (!observed.isNull()) {
    clogger() << JASSTR(" - [expected]: ") << JsonTrait::dump(tc.expected)
              << JASSTR(" - [observed]: ") << observed.dump();
  } else {
    clogger() << " - [reason]: \n" << reason;
  }
  clogger() << "\n";
}

static void success_test_case(const test_case& /*tc*/) { ++total_passes; }

static Json parse_test_case(const String& data) {
  return JsonTrait::parse(data);
}

static EvalContextPtr make_eval_ctxt(Json data) {
  if (JsonTrait::isObject(data) && JsonTrait::hasKey(data, JASSTR("__old")) &&
      JsonTrait::hasKey(data, JASSTR("__new"))) {
    return std::make_shared<HistoricalEvalContext>(
        nullptr, JsonTrait::get(data, JASSTR("__new")),
        JsonTrait::get(data, JASSTR("__old")));
  } else {
    return std::make_shared<HistoricalEvalContext>(nullptr, std::move(data));
  }
}

}  // namespace jas

using namespace jas;
int main(int argc, char** argv) {
  CloggerSection test{JASSTR("JAS TEST")};
  if (argc == 2) {
    return jas::run_all_tests(argv[1]);
  } else {
    clogger() << "ERROR: No test case dir specified!";
    return -1;
  }
}
