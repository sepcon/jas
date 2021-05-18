#include <boost/any.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/include/qi.hpp>
#include <iostream>
#include <map>
#include <vector>

namespace json {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

struct nullptr_t_ : qi::symbols<char, void*> {
  nullptr_t_() { add("null", nullptr); }
} nullptr_;

template <typename Iterator>
struct Grammar : qi::grammar<Iterator, boost::any(), ascii::space_type> {
  Grammar() : Grammar::base_type(start) {
    using ascii::char_;
    using qi::bool_;
    using qi::double_;
    using qi::lexeme;

    start = value_rule.alias();
    object_rule = '{' >> pair_rule % ',' >> '}';
    pair_rule = string_rule >> ':' >> value_rule;
    value_rule =
        object_rule | array_rule | string_rule | nullptr_ | double_ | bool_;
    array_rule = '[' >> value_rule % ',' >> ']';
    string_rule = lexeme['\"' >> *(char_ - '\"') >> '\"'];
  }

  qi::rule<Iterator, boost::any(), ascii::space_type> start;
  qi::rule<Iterator, std::map<std::string, boost::any>(), ascii::space_type>
      object_rule;
  qi::rule<Iterator, std::pair<std::string, boost::any>(), ascii::space_type>
      pair_rule;
  qi::rule<Iterator, boost::any(), ascii::space_type> value_rule;
  qi::rule<Iterator, std::vector<boost::any>(), ascii::space_type> array_rule;
  qi::rule<Iterator, std::string(), ascii::space_type> string_rule;
};

}  // namespace json

int main() {
  const std::string source = R"_json([ null, 1, 2, "hello" ])_json";
  json::Grammar<std::string::const_iterator> g;
  boost::any v;
  auto bit = source.begin();
  auto eit = source.end();
  bool r = boost::spirit::qi::phrase_parse(bit, eit, g,
                                           boost::spirit::ascii::space, v);
  if (r) {
    auto a = boost::any_cast<std::vector<boost::any> >(v);
    for (auto it = a.begin(); it != a.end(); ++it) {
      std::cout << boost::any_cast<void*>(*it) << std::endl;
    }
  }
  return 0;
}
