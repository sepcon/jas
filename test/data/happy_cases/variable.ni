{"expr":{"@plus": [1, 2], "$": "sum"}, "sum": "$sum"}
{"expr":3,"sum":3}
{"$variable":{"int":10,"object":{"string":"helloworld"}},"valof_object.string":"$variable[object/string]"}
{"valof_object.string":"helloworld"}
{"$field_string":"string","$variable":{"int":10,"object":{"string":"helloworld"}},"valof_object.string":"$variable[object/$field_string]"}
{"valof_object.string":"helloworld"}
