{"@transform":{"@list":[1,2,3,4],"@op":"@to_string:@field"}}
["1", "2", "3", "4"]
{"@transform":{"@list:@filter_if":{"@list":[1,2,3,4],"@cond:@eq":[{"@modulus":["@field",2]},1]},"@op":"@to_string:@field"}}
["1", "3"]
{"@transform":{"@list":[1,2,3,4],"@op":{"is_odd":"@is_odd:@field","as_string":"@to_string:@field"}}}
[{"as_string": "1", "is_odd": true}, {"as_string": "2", "is_odd": false}, {"as_string": "3", "is_odd": true}, {"as_string": "4", "is_odd": false}]
