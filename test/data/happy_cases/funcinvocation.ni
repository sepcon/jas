"@tolower:HELLOWORLD"
"helloworld"
"@toupper:helloworld"
"HELLOWORLD"
{"@gt": ["@current_time", 1621417144] }
true
"@unix_timestamp:2021/06/04 02:43:20"
1622749400
{"@lt":["@unix_timestamp", 0]}
{"@exception": "EvaluationError"}
{"@contains":["Hello", "ell"]}
true
{"@contains":["Hello", "bell"]}
false
{"@to_string":100.001}
"100.001"
{"@to_string":"100.001"}
"\"100.001\""
{"@has_null_val":{"not_null": 1}}
false
{"@has_null_val":{"null": null}}
true
"@has_null_val:hello"
false
{"@has_null_val":null}
true
{"@cmp_ver":["6.1.7600.16385","6.1.7600.16385"]}
0
{"@cmp_ver":["6.1.7600.16386","6.1.7600.16385"]}
1
{"@cmp_ver":["6.1.7600.16386","6.1.7601.16385"]}
-1
{"@cmp_ver":["6.2.7600.16386","6.1.7601.16385"]}
1
{"@cmp_ver":["6.2.7600.16386","7.1.7601.16385"]}
-1
{"@eq_ver":["6.2.7600.16386","7.1.7601.16385"]}
false
{"@ne_ver":["6.2.7600.16386","7.1.7601.16385"]}
true
{"@gt_ver":["6.2.7600.16386","7.1.7601.16385"]}
false
{"@lt_ver":["6.2.7600.16386","7.1.7601.16385"]}
true
{"@ge_ver":["6.2.7600.16386","7.1.7601.16385"]}
false
{"@le_ver":["6.2.7600.16386","7.1.7601.16385"]}
true
{"@le_ver":["6.2.7600.16386","6.2.7600.16386"]}
true
{"@ge_ver":["6.2.7600.16386","6.2.7600.16386"]}
true
{"@match_ver": ["1.1.2", "1.x"]}
true
{"@match_ver": ["1.1.2", "2.x"]}
false
{"@match_ver": ["1.1.2", ["1.x", "2.x"]]}
true
{"@match_ver": ["1.1.2", "2.x", "3.x", "1.x" ]}
true
{"@match_ver": ["1.1.2", ["3.x", "2.x"], ["1.x"] ]}
true
{"@gt": ["@current_time_diff:1628174873(%d)", 1000]}
true
