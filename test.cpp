
#include "jas_parser.h"
#include "jas_syntax_generator.h"

namespace jas {

namespace sg_test {

using test_case_data = std::pair<string_type, string_type>;
using test_data = std::vector<test_case_data>;

const test_data& _test_data() {
  static test_data _d = {
    {R"({"@none_of":{"@list_path":"","@item_id":"ID","@cond":{"@ge":[{"@minus":[{"@func":"current_time"},{"@key":"data_file_time"}]},259200]}}})",
     R"_(none_of("").satisfies(((call(current_time) - value_of("data_file_time")) >= 259200)))_"},
};

return _d;
}  // namespace sg_test

}  // namespace jas

}  // namespace jas
