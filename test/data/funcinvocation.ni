"@tolower:HELLOWORLD"
"helloworld"
"@toupper:helloworld"
"HELLOWORLD"
{"@gt": ["@current_time", 1621417144] }
true
"@unix_timestamp:2021/06/04 02:43:20"
1622749400
{"@lt":["@unix_timestamp", 0]}
true
{"@contains":["Hello", "ell"]}
true
{"@contains":["Hello", "bell"]}
false
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
{"@empty":{}}
true
{"@not_empty":{}}
false
{"@empty":[]}
true
{"@not_empty":[]}
false
{"@empty":[1]}
false
{"@not_empty":[1]}
true
{"@empty":{"k":1}}
false
{"@not_empty":{"k":1}}
true
{"@abs":-1}
1
{"@abs":10}
10
{"@is_odd":10}
false
{"@is_odd":11}
true
{"@is_even":10}
true
{"@is_even":11}
false
{"@len": [1,2,3]}
3
{"@len":{"k1":1,"k2":2,"k3":3}}
3
{"@len":{}}
0
{"@len":"1234567890"}
10

