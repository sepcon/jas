{"@alg.sort":[[{"id":1},{"id":2},{"id":3}],{"@gt":["$1[id]","$2[id]"]}]}
[{"id":3},{"id":2},{"id":1}]
{"@alg.sort":[[{"id":1},{"id":2},{"id":3}],{"@plus":["$1[id]","$2[id]"]}]}
{"@exception": "TypeError"}
{"@filter": [[1,2,3], {"@eq": [0, {"@modulus":["$1", 2]}]}]}
[2]
{"@alg.transform": [[1,2,3], {"@plus": [1, "$1"]}]}
[2,3,4]
{"$thelist":[100,200,300,110],"@alg.any_of":["$thelist",{"@gt":["$1",199]}]}
true
{"$thelist":[],"@alg.any_of":["$thelist",{"@gt":["$1",199]}]}
false
{"$thelist":[],"@alg.none_of":["$thelist",{"@gt":["$1",199]}]}
true
{"$thelist":[],"@alg.all_of":["$thelist",{"@gt":["$1",199]}]}
true
{"$thelist":[100,200,300,110],"@alg.count_if":["$thelist",{"@gt":["$1",199]}]}
2
{"$thelist":[100,200,300,110],"@alg.count_if":["$thelist",{"@gt":["$1",399]}]}
0
{"$thelist":[],"@alg.count_if":["$thelist",{"@gt":["$1",399]}]}
0
