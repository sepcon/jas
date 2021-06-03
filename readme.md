

# Json evAluable Syntax [JAS]
## Purposes
- Easy to parse evaluable syntax written in json
- A set of expressions can be specified from file/network and change flexibly without modification from source code
- Can be used to evaluate some kind of json-like data then determine further actions accordingly

## Definition
-	Everything is considered as Evaluable Expression and finally will be evaluated to a ***consumable value( json)*** or an exception will be thrown if an expression cannot be evaluated(invalid syntax, function not supported...)
-	Evaluable expression can be specified as input/arguments of other expression recursively and the nested one will always be evaluated first
- 	The expressions will be applied on a(pair) of json(like) data and corresponding data will form a ***working context***, the currently support only 2 contexts as last/last-snapshot data
###  Syntax:
  >     (1): {"@specifier": Evaluable, "#": "property_name"}
  >     (2): "@specifier[:@specifier1][:@specifier2]...[:@specifierN | string]"
  >     (3): "#property_name"
  > 	(4): {"key1[:@specifier1],...[:@specifierN|string]": Evaluable, "key2": ...}
  > 	(5): [Evaluable1, Evaluable2... EvaluableN]
- **specifier** is a `keyword` that specifies an operation or a pre-defined parameter of an operation or a supported function. A specifier must begin with a `@` letter.
- **[argument]**  specifies input/argument of operation that specified by corresponding `specifier`. `argument` will always be evaluated once before passing to operation/parameter specified by `specifier` 
- **property_name** evaluated result of corresponding expression will be assigned to a property then can be queried later by expression (3):  `"#property_name"`
- **Syntax (1):**  if `specifier` specifies an operation, or a function, the given syntax must be a json_object that has only single `specifier` and will be evaluated to single value, if having more than one `specifier`, the first one will be evaluated and the rest will be ignored.
- **Syntax (2):** Specifier must be an operation/function that will be evaluated to a single value, `specifierN` will be evaluated as `argument` of `specifierN-1`, the last argument can be a `string` and `string` will be directly pass to corresponding operation 
- **Syntax (3):** queries corresponding property that previously set by `"#": "property_name"` in **_syntax (1)_**
- **Syntax (4):** evaluates to a json_object looks like: `{ "key1" : value1, "key2": "value2" ...}`
- **Syntax (5):** evaluates to a json_array looks like: `{ value1, value2, ..., valueN}`
- **_Example:_** 
   > ```
   > {"@plus": [1, 2, 3] }   ==> 6
   > {"@toupper" : "@field:name" } ==> "SOME_NAME"
   > {"@plus": [1,2], "@minus": [1,2], "#": "plus_result" }  ==> -1 // @plus peration will be ignored, because  minus < plus in string comparison
   > "#plus_result" => -1
   > "#not_assigned_property" ==>  null
   > { "value1": "@current_time", "value2": { "@plus": [1, 2, 3] } } ==> { "value1": 1623834906, "value2":  6 }
   > [ "#plus_result", "@current_time", "@to_string:@current_time" ] ==> [-1, 1623834906, "1623834906" ]
   >```
   
### Classification:
- **Evaluable:**`<evaluable>` abstract concept of an `operation` or  `consumable value` that will finally be  evaluated to a consumable value
- **Consumable:**  `Json: number(floating point, integer), string, boolean, object, array`, 
	- **Note**: a `string` must not begin with `@` letter and should not contains any `":@` sub-string in middle, if not it will be evaluated as an expression:
	- **Example:** `"value:@current_time` will be reconstructed as `{"value" : "@current_time"}`
- **Arithmetical operators:** `<arthm_op>`
	- **Description:** Apply arithmetical operator on on array of Evaluables that evaluated to **_values of same type_**
	- **Syntax:** `{ "@operator": [arg1, arg2, ..., argN] }`
		- `@operator:`
			- `@bit_and` : `a & b` -> **integer** only
			- `@bit_not`: `~a` -> **integer** only
			- `@bit_or`: `a|b` -> **integer** only
			- `@bit_xor`: `a^b` -> **integer** only
			- `@divides`: `a/b` -> **number** only
			- `@minus`: `a-b` -> **number** only
			- `@modulus`: `a%b` -> **integer** only
			- `@multiplies`: `a*b` -> **integer** only
			- `@negate`: `-b` -> **number** only, 
			- `@plus`: `a+b` -> **number** and **string**
		- `[arg1, arg2, ..., argN]`: array of Evaluables that evaluated to json_array of same-type elements
		-  `supported types:` Number(integer/floating point) and `@plus` can be used for string concatenation as well
		- `result type`: same as first argument
	- **Example:**
		- Valid:
			- `{"@plus":[1,2, ..., N]}` => `1 + 2 + ... + N`
			- `{"@plus":["hello", "world", "opswat client"]}` => `"hello" + "world" + "opswat client"`
			- `{"@negate":[1]}` => `-1`
		- Invalid
			- `{"@plus":[1,true]}` -> arguments with mismatched types
			-  `{"@plus":[1]}` -> missing 1 argument
- **Logical operators**: `<logical_op>`
	- **Description:** Apply logical operator list/array of `Evaluables` that evaluated to boolean/number values
	- **Syntax:** `{ "@operator": [arg1, arg2, ..., argN] }`
		- `@operator`:
			- `@and` : `a && b` 
			- `@or`: `a||b` 
			- `@not`: `!b` 
		- `[arg1, arg2, ..., argN]`: array of Evaluables that evaluated to json_array of same-type of `integer/boolean`
		- `supported types:` boolean/number 
		- `result type:` Same type as parameters `(eval(integer+integer) = integer)`
		
	- **Example:**
		- Valid:
			- `{"@and":[true, true, false]}` => `true && true && false`==> false
			- `{"@or":[1, 2]}` => `1 || 2`==> 1
		- Invalid
			-  `{"@and":[1]}` -> missing 1 argument
			-  `{"@and":[1, true]}` -> type mismatched
- **Comparison operators**: `<comparison_op>`
	- **Description:** Apply logical operator list/array of `exactly two Evaluables` that evaluated to `Consumable`value
	- **Syntax:** `{ "@operator": [arg1, arg2] }`
		- `@operator`:
			- `@eq` : `a == b` 
			- `@neq` : `a != b` 
			- `@gt`: `a>b` 
			- `@ge`: `a>=b` 
			- `@lt`: `a<b` 
			- `@le`: `a<=b` 
		- `[arg1, arg2]`: array of Evaluables that evaluated to json_array of **exactly two** same-type elements 
		- `supported types`: `json` 
		- `result type`: `boolean`
	- **Example:**
		- Valid:
			- `{"@eq":[1, 1]}` => `1 == 1`==> true
			- `{"@gt":["abc", "abcd"]}` => `"abc" > "abcd"` ==> false
		- Invalid
			-  `{"@eq":[1]}` -> missing 1 argument
			-  `{"@eq":[1, "str"]}` -> type mismatched
			-  `{"@eq":[1, 2,4] }`: -> more than 2 args
- **List operations:**`<list_op>`
	- **Description:** Checks that a criteria/condition is satisfied on [one/all] item(s) of a given  list/array or not
	- **Syntax:** 
	  >`(1){"@operation":{"@cond": boolean, "@list": array}}`
	  
		- `@cond`: `[Evaluable]`that must be evaluated as **boolean**
		- `@op`: `[Evaluable]` used for `@transform` operation, 
		- `@list`: `[Evaluable]` spmust be evaluated to array.
		- `@item_id`: `[string]`: specifies unique field of a list item. Use this when want to compare corresponding values between two lists. if empty, item in 2 lists will be mapped by index.
		- `Available operations`: 
			- `@any_of` - `[return:boolean]`: evaluated to `true` if `@cond`is evaluated to `true`on any item of given list
			- `@all_of` - `[return:boolean]`: evaluated to `true` if `@cond`is evaluated to `true`on all items of given list
			- `@none_of` - `[return:boolean]`: evaluated to `true` if `@cond`is evaluated to `false`on all items of given list
			- `@count_if` - `[return:integer]`: evaluated to number of items that satisfies `@cond`
			- `@filter_if` - `[return:array]`: pick the items that satisfy `@cond`
			- `@transform` - `[return:array]`: transforms each item in the given list to different form by Evaluable `@op`
			- **NOTE:** to check a list is empty we can specify as: `{"@any_of" : true} or {"@any_of": {"@list": "$field:the_list", "@cond" : true}}`
	- **Example:**
		- Valid:
			> ```
			> {
			>  "@any_of": {
			>    "@list": "anti-malware",
			>    "@item_id": "ID",
			>    "@cond": {
			>      "@eq": [ "@field:rtp_enabled",  1 ]
			>   }
			>  }
			>}

           > `"@size_of: [1,2,3]"` => 3
           
           > `{"@count_if":{ "@list": [1,2,3,4], "@cond:@gt": [ "@field", 2] }}` => 2
		- Invalid
			> ```
			> {
			>  "@any_of": {
			>    "@path": "anti-malware",
			>    "@cond": "@current_time"
			>  }
			>}
			> -> why: @current_time will be evaluated to integer instead of boolean  
	 
- **EvaluableArray:**
	- **Description:** specifies an array of Evaluables. output will be array of corresponding Evaluated value
	- **Sample:** `[{"@plus": [1,2]}, "@to_string:100"]` --> `[3, "100"]`
	
- **EvaluableMap:**
	- **Description:** Similar to EvaluableArray, that specifies mapping of Keys-Evaluables. Output will be JsonObject
	- **Sample:** 
		>```
		> { 
		>	"one_and_two": {"@plus": [1,2]},
		>	"one_hundred": "@to_string:100"
		> }
		> --> 
		> { 
		>	"one_and_two": 3,
		>	"one_hundred": "100"
		> }
		>```

- **Context/ContextProperty:**
	- **Description:** 
		- **Context**: Each Evaluable is evaluated in a `context` where having its own evaluated results, and cached results for re-use
		and differentiate with other `context`. Like a function in other programming languages(C++/Java...)
		each one has a context to specify the variables for later computation.
		
		- **ContextProperty:** is similar variable in function and has accessing scope. it is designed to store constant value 
		or evaluated result from other Evaluable for accessing later by other Evaluables.
			- ContextProperty(Property) can be declared inside the definition of an Operation/Function/EvaluableMap
			- Property has its own scope, and can only be accessed from its/its-sub scopes(Context)
			- If Property is not found in current context(scope), it will be looked up in parent context until root context
			- Property can be declared to be visible in global context for accessing later by Evaluables in all other contexts
	- **Syntax:** `$property_name:Evaluable` 
		- ***NOTE:*** this identical to Property specified by `#` in above section, and they can be used interchangable.
		- To store a variable in global(root) context, use `$$`
		- use `$property_name` to access the value that set in `property_name`
	- **Example:**
	>```
	> {
	> 	"$cur_time": "@current_time",
	> 	"expired_duration:@minus": ["$cur_time", "@field:last_update_time"]
	> 	"expired_duration1:@minus": ["#cur_time", "@field:last_update_time"] // similar with #
	> } 
	> --> might results in:
	> {
	>	"expired_duration": 64800
	> }```
	
		
- **Function**:
	- **Description:**  invokes a function that supported by default or by `current_context`
	- **Syntax:**
		> ```
		> (1){ "@function_name":  Evaluable}
		> (2) "@function_name[:@function_name1]...[:@function_nameN/string]"
		
		- `@function_name`: name of function to be invoked, if `function_name` does not supported by default or by current context, exception will be thrown.
		- `Evaluable`: an evaluable syntax that will be evaluated as `function`'s argument(s)
	- **Example:**
		- `"@current_time"`
		- `{"@cmp_ver": ["1.2.123", "1.19.123"]}`
	- **Predefined functions**:
		> - `@current_time`:  `(->integer)` get unix-time timestamp of system_clock 
		> - `@tolower: string`: `(->string)`converts a string to lower case
		> - `@tolupper: string`: `(->string)` converts a string to upper case
		> - `@snchg` `(->boolean)` return true if last/cur data snapshot in current context are different
		> - `@prop: string` : `(->json)`gets value of an assigned property - same as "#property_name"
		> - `@cmp_ver: ["v1", "v2"]`:  `(->integer)`compare the strings as versions(`-1: v1 < v2, 0: equal, 1: v1>v2`)
		> - `@eq_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 == v2`
		> - `@ne_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 != v2`
		> - `@gt_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 > v2`
		> - `@lt_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 < v2`
		> - `@ge_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 >= v2`
		> - `@le_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 <= v2`
		> - `@contains: ["s1", "s2"]`:`(->boolean)` true if `s2` appears in `s1`
		> - `@unix_timestamp: string`:  `(->integer)`converts a string that represents a date_time to unix 
		time_stamp
		> - `@has_null_val: array/object`:`(->boolean)` true if given array/object contains any null value
		> - `@size_of: Evaluable`: `(->unsigned integer)` return the size of given object/array
		> - `... add more ...`
	
	- **Special functions**: Bellow functions are for accessing historical input data 
		> - `@hfield`: select the data of field that contains both last snapshot and current snapshot, return of this function must not be consume dirrectly by evaluable operations
		>	- input: 
		>		- `[string]`: path to the field. E.g: `/Antivirus/product/name`
		>		- `[JsonObject]: {"path": "path/to/field", "iid": "item_id"}`, for field that points to a list only
		>	- return: `JsonObject`
		> - `@hfield2arr`: shorthand for `@hfield`, for case field points to an array
		>	- input:  `["path/to/field", "iid": "item_id"]` ->  array contains 2 elements
		>	- return: `JsonObject`
		>	- E.g: `{"@hfield2arr": ["Antivirus/Threats", "ID"]}`
		> - `@field`: select a field, the result contains value of current or last snapshot only
		>	- input:
		>		- `String: path/to/field` -> will select value of field in current snapshot
		>		- `Object: { "path": "path/to/field", "snapshot": "last/cur"}` -> select the field in specific snapshot
		>	- return: `Json`
		> - `@field_cv`: select value of field in current snapshot - more explicit than field
		>	- input: `String: path/to/field`
		>	- return: `Json`
		> - `@field_lv`: select value of field in last snapshot - shorthand for selecting field in last snapshot
		>	- input: `String: path/to/field`
		>	- return: `Json`
		>```


	