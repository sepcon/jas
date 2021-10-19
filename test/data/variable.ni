{"$.variable":{"int":10,"object":{"string":"helloworld"}},"valof_object.string":"$.variable[object/string]"}
{"valof_object.string":"helloworld"}
{"$field_string":"string","$variable":{"int":10,"object":{"string":"helloworld"}},"valof_object.string":"$variable[object/$field_string]"}
{"valof_object.string":"helloworld"}
{"$aint:@plus":["$bint",100],"$bint:@plus":[3,1],"$afinal:@divides":[{"@minus":[10,{"@plus":[100,{"@plus":["$bint","$aint"]}]}]},100],"Final":"$afinal"}
{"Final":-1.98}
// $a cyclic reference with $b
{"$b:@plus":["$a", 2],"$a:@plus":["$b", 1]}
{"@exception": "EvaluationError"}
{"$arr":[1,2,3], "@return": "$arr[3]"}
null
{"$field_arr":"arr","$item3_idx":2,"$object":{"arr":[{"name":"item1"},{"name":"item2"},{"name":"item3"}]},"myquery":"$object[$field_arr/$item3_idx/name]"}
{"myquery":"item3"}
"$variable:1(%d)"
1
"text:$variable:1"
{"text": "1"}
