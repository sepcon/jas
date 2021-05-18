
#include <filesystem>
#include <fstream>
#include <iostream>

#include "JEvalContext.h"
#include "Json.h"
#include "Parser.h"
#include "SyntaxEvaluator.h"
#include "SyntaxValidator.h"

namespace jas {
using json11::Json;
namespace fs = std::filesystem;

struct test_case {
  Json expected;
  Json rule;
  Json context_data;
  int data_line_number = -1;
};

using test_cases = std::vector<test_case>;
struct data_error : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

static int run_all_tests(const fs::path& testcase_dir);
static void begin_test(const fs::path& test_data_file);
static void end_test(const fs::path& test_data_file);
static test_cases load_no_input_test_cases(const fs::path& data_file);
static test_cases load_has_input_test_cases(const fs::path& data_file);
static void run_test_case(const test_case& tc);
static void failed_test_case(const test_case& tc, const std::string& syntax,
                             const json11::Json* observed,
                             const String& reason = {});
static void success_test_case(const test_case& tc);
static Json parse_test_case(const std::string& data);

static int total_passes = 0;
static int total_failed = 0;

static int run_all_tests(const fs::path& testcase_dir) {
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
        begin_test(test_data_file);
        try {
          auto tcs = load_test_cases(test_data_file);
          for (auto& tc : tcs) {
            try {
              run_test_case(tc);
            } catch (const std::runtime_error& e) {
              failed_test_case(tc, {}, {}, e.what());
            }
          }
        } catch (const data_error& de) {
          std::cout << "Failed to parse test case: " << de.what() << std::endl;
        }
        end_test(test_data_file);
      }
    }
  }
  std::cout << "\nSUMARY:"
            << "\nTotal passes: " << total_passes
            << "\nTotal failed: " << total_failed << std::endl;
  return total_failed;
}
static void begin_test(const fs::path& test_data_file) {
  std::cout << "start testing file " << test_data_file.filename() << std::endl;
}
static void end_test(const fs::path& test_data_file) {
  std::cout << "finished testing file " << test_data_file.filename()
            << std::endl;
}

static test_cases load_no_input_test_cases(const fs::path& data_file) {
  std::ifstream ifs{data_file};
  std::string line;
  int i = 0;
  int line_number = 0;
  Json rule;
  Json expected;

  test_cases tcs;
  while (std::getline(ifs, line)) {
    ++line_number;
    if (line.find("//") == 0) {
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
  std::ifstream ifs{data_file};
  std::string line;
  int i = 0;
  int line_number = 0;
  Json rule;
  Json input;
  Json expected;

  test_cases tcs;
  while (std::getline(ifs, line)) {
    ++line_number;
    if (line.find("//") == 0) {
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
    auto evaluable = parser::parse(tc.rule);
    if (evaluable) {
      auto syntax = SyntaxValidator{}.generate_syntax(*evaluable);

      auto evaluated = SyntaxEvaluator{}.evaluate(
          evaluable, std::make_shared<JEvalContext>(nullptr, tc.context_data));
      if (JsonTrait::equal(tc.expected, evaluated.value)) {
        success_test_case(tc);
      } else {
        failed_test_case(tc, syntax, &evaluated.value);
      }
    } else {
      failed_test_case(tc, "", nullptr, "Failed to parse data");
    }
  } catch (const ExceptionBase& e) {
    failed_test_case(tc, JsonTrait::dump(tc.rule), nullptr, e.what());
  }
}

static void failed_test_case(const test_case& tc, const std::string& syntax,
                             const Json* observed, const String& reason) {
  ++total_failed;
  std::cout << "TC[" << tc.data_line_number << "][FAILED] - syntax: " << syntax;
  if (observed) {
    std::cout << " - [expected]: " << tc.expected.dump()
              << " - [observed]: " << observed->dump();
  } else {
    std::cout << " - [reason]: \n" << reason;
  }
  std::cout << "\n";
}

static void success_test_case(const test_case& /*tc*/) {
  ++total_passes;
  //  std::cout << "TC[" << tc.data_line_number << "][SUCCESS]\n";
}

static Json parse_test_case(const std::string& data) {
  std::string json_parsed_err;
  auto parsed = Json::parse(data, json_parsed_err);
  if (!json_parsed_err.empty()) {
    throw data_error{json_parsed_err};
  }
  return parsed;
}
}  // namespace jas

int main(int argc, char** argv) {
  if (argc == 2) {
    return jas::run_all_tests(argv[1]);
  } else {
    std::cout << "ERROR: No test case dir specified!" << std::endl;
    return -1;
  }
}
