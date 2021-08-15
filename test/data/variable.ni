{"expr":{"@plus": [1, 2], "$": "$sum"}, "sum": "$sum"}
{"expr":3,"sum":3}
{"$variable":{"int":10,"object":{"string":"helloworld"}},"valof_object.string":"$variable[object/string]"}
{"valof_object.string":"helloworld"}
{"$field_string":"string","$variable":{"int":10,"object":{"string":"helloworld"}},"valof_object.string":"$variable[object/$field_string]"}
{"valof_object.string":"helloworld"}
{"$aint:@plus":["$bint",100],"$bint:@plus":[3,1],"$afinal:@divides":[{"@minus":[10,{"@plus":[100,{"@plus":["$bint","$aint"]}]}]},100],"Final":"$afinal"}
{"Final":-1.98}
// $a cyclic reference with $b
{"$b:@plus":["$a", 2],"$a:@plus":["$b", 1]}
{"@exception": "EvaluationError"}
