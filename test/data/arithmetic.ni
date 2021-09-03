{"@bit_and":[1,2]}
0
{"@bit_and":[1,3]}
1
{"@bit_or":[1,2]}
3
{"@bit_not":[2]}
-3
{"@bit_xor":[1,2]}
3
{"@divides":[10,2]}
5
{"@minus":[1,2]}
-1
{"@modulus":[3,2]}
1
{"@multiplies":[1,2]}
2
{"@negate":[3]}
-3
{"@plus":[1,2]}
3
{"@bit_and":[1,3,5]}
1
{"@bit_or":[1,2,3]}
3
{"@bit_xor":[1,2,3]}
0
{"@divides":[10,2,5]}
1
{"@minus":[1,2,3]}
-4
{"@modulus":[4,2,1]}
0
{"@multiplies":[1,2,3]}
6
{"@plus":[1,2,3]}
6
{"@plus":["a","b"]}
"ab"
{"@plus": [1, 1.2, 1.3]}
3.5
{"$var": true, "@s_bit_or": ["$var", false]}
true
{"$var": true, "@s_bit_and": ["$var", false]}
false
{"$var": true, "@s_bit_xor": ["$var", false]}
true
{"$var": true, "@s_bit_xor": ["$var", true]}
false
{"$var": false, "@s_bit_xor": ["$var", false]}
false
{"$var": 1.0, "@s_plus": ["$var", 1.0]}
2
{"$var": 1.0, "@s_minus": ["$var", 1.0]}
0
{"$var": 1.0, "@s_multiplies": ["$var", 5.0]}
5
{"$var": 1.0, "@s_divides": ["$var", 5.0]}
0.2
{"$var": 1, "@s_modulus": ["$var", 5]}
1
{"$var": 5, "@s_modulus": ["$var", 5]}
0
{"@plus": [1,"2"]}
{"@exception": "EvaluationError"}
{"@plus": [1,true]}
{"@exception": "EvaluationError"}
