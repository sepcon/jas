{"@eq":[1,1]}
true
{"@eq":[0,1]}
false
{"@ge":[1,1]}
true
{"@ge":[2,1]}
true
{"@ge":[0,1]}
false
{"@gt":[1,0]}
true
{"@gt":[1,1]}
false
{"@gt":[0,1]}
false
{"@le":[1,0]}
false
{"@le":[1,1]}
true
{"@le":[0,1]}
true
{"@lt":[0,1]}
true
{"@lt":[1,1]}
false
{"@lt":[1,0]}
false
{"@neq":[0,1]}
true
{"@neq":[1,1]}
false
//string comparison
{"@eq":["1","1"]}
true
{"@eq":["0","1"]}
false
{"@ge":["1","1"]}
true
{"@ge":["2","1"]}
true
{"@ge":["0","1"]}
false
{"@gt":["1","0"]}
true
{"@gt":["1","1"]}
false
{"@gt":["0","1"]}
false
{"@le":["1","0"]}
false
{"@le":["1","1"]}
true
{"@le":["0","1"]}
true
{"@lt":["0","1"]}
true
{"@lt":["1","1"]}
false
{"@lt":["1","0"]}
false
{"@neq":["0","1"]}
true
{"@neq":["1","1"]}
false
{"@neq":[null, 1]}
true
{"@neq":["string", 1]}
true
{"@eq":["string", [1,2,3]]}
false
{"@gt": [1.2, 1]}
true
{"@eq": [[1,2],[1,2]]}
true
{"@eq": [[1,3],[1,2]]}
false
{"@lt": [[1,3],[1,2]]}
false
{"@gt": [[1,3],[1,2]]}
true
