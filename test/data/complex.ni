{"@eq":[{"@minus":[10,20]},{"@plus":[-1,-9]}]}
true
{"@eq":[{"@minus":[11,20]},{"@plus":[-1,-9]}]}
false
{"@or":[{"@eq":[{"@minus":[10,20]},{"@plus":[-1,-9]}]},{"@eq":["a","b"]}]}
true
{"@or":[{"@eq":[{"@minus":[10,20]},{"@plus":[-1,-9]}]},{"@eq":["a","a"]}]}
true
{"@or":[{"@eq":[{"@minus":[10,20]},{"@plus":[-1,-8]}]},{"@eq":["a","b"]}]}
false
{"@plus": [10, "@field:nothing"]}
{"@exception":"EvaluationError"}
