{"$dict":{"item1":1},"$dict1":{"item2":"string"},"@dict.update":["$dict","$dict1"]}
{"item1":1,"item2":"string"}
{"$dict":{},"$dict1":{"item2":"string","item1":1},"@dict.update":["$dict","$dict1"]}
{"item1":1,"item2":"string"}
{"$dict":{"item1":1},"@dict.update":["$dict",1,2]}
{"@exception":"InvalidArgument"}
{"$dict":{},"@dict.update":["$dict","key","val"]}
{"key":"val"}
{"$dict":{},"@dict.update":["$dict","key","val", "unexpected"]}
{"@exception":"InvalidArgument"}
{"$object":{"key1":1,"key2":2,"key3":3},"@dict.keys":"$object"}
["key1","key2","key3"]
{"$object":{"key1":1,"key2":2,"key3":3},"@dict.values":"$object"}
[1,2,3]
{"@dict.keys":{"key1":1,"key2":2,"key3":3}}
["key1","key2","key3"]
{"@dict.values":{"key1":1,"key2":2,"key3":3}}
[1,2,3]
{"@dict.clear":{"key":"value"}}
{}
{"@dict.get":[{"key":"value"}, "key"]}
"value"
{"@dict.get_path":[{"key1":{"key2":{"key3":"value"}}},"key1/key2/key3"]}
"value"
{"@dict.erase":[{"key":"value"}, "key"]}
{}
{"@dict.get":[{"key":"value"}, 1]}
{"@exception":"InvalidArgument"}
{"@dict.erase":[{"key":"value"}, 1]}
{"@exception":"InvalidArgument"}
{"@dict.is_empty":{}}
true
{"@dict.is_empty":{"key":"value"}}
false
{"@dict.contains":[{"key":"value"}, "key"]}
true
{"@dict.contains":[{"key":"value"}, "key1"]}
false
{"@dict.exists":[{"key":"value"}, "key"]}
true
{"@dict.exists":[{"key1":{"key2":{"key3":"value"}}},"key1/key2/key3"]}
true
