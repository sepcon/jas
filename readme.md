
# Json evAluable Syntax [JAS]
## Purposes
- Easy to parse using the backend is Json parser
- A set of instructions can be specified from file/network and change flexibly without modification from source code
- Can be used to evaluate some kind of json-like data then determine further actions accordingly

## Concepts: 
**Everything is considered as`Evaluable` , includes `constant, variable, operation`**  and will be evaluated to consumable values
 with types specified by following **`Value Type`** section.
- **Value types**: Null, String, Boolean, Number(Integer/Double), List(json array), Dict(Json object), Reference. 
Reference refer to a variable that holding some value, then can be used to modify the referred one.
	- **Consumable**: After evaluation, every Evaluable will be evaluated to one of listed Value types called consumable
- **Constant**: Is any value that evaluates to it-self or specified by operation `@noeval`:
	- Example: `10, "hello", [1,2,3], {"address": "street, h.no..."} `
- **Statement:** an expression can be a `value` or a `statement` that specifies an operation or declares a `variable`
	- **Operation:**  includes `arithmatical/comparison/logical operators, list algorithms, function invocation`. Operation looks like a function call in other programing languages, that has a name, parameters(input) and return type.
		- ***Syntax***:  An operation has a name precedes with letter `@` and following by  one or zero input.
			-  Form1: A `json object` that contains unique pair of`@key:value` with `key` is the operation name. The `value` is  is an `Evaluable` type and is the input of the operation. Eg:  `{"@plus":[1,2]}`, `{"@to_string": "@current_time"}` 
			- Form2: A String that specifies an operation that accept zero or one input. Eg: `"@current_time"`, `"@to_string:@current_time"`
	- **Variable**: is a mean to store some evaluated value for reuse later. Similar with the term variable in other programing languages.
		- **Syntax**:  a pair of `$key:value` that appears in context of an `Evaluable`. the `key` is variable name that precedes with letter `$`, the `value` is an `Evaluable` expression that assigned to the variable.
			***- Initialization:*** `"$variable_name": Evaluable` Variable will be declared and store in current `context`(see concept of context below), and will be visible in all sub-contexts, and might be hidden by same variable name in sub-context.
			A variable can be declare in root context(like global variable in other languages) and visible with all sub-contexts by preceding variable with two letters `$$`. Eg: 1. local: `"$curtime":"@current_time"`, 2. global:`"$$curtime":"@current_time"`
			***- Assignment:*** `"$+variable_name": Evaluable`,  with `+` sign follows letter`$`. Assignment differs with Initialization by not try to create and store variable in any context, it searches for existing variable in current context or parent contexts until root. If not found, `EvaluationError` exception will be thrown. Eg: `{"$var": 1, "$+var:@plus":[1,2]}` 
	- **Context**: Each operation will be evaluated in a context, in that many variables will be declared for taking part into evaluation of the operation. A context can contain ONLY ONE operation, if more than one operations appear, the first one will be evaluated and later ones are omitted. The context might contain unlimited number of variables. Eg: 
		>```
		>{
		> "$first": 1, "$second":2,
		> "@plus": ["$first", "$second"]
		>} --> evaluated to 3
		
		- Evaluation always starts with a root context, then each following operation will be locally evaluated in a sub-context.
		- A context can be used to evaluate to a dictionary(json object) for passing as input of other operation. Eg:
		>```
		>{
		>	"@date_time": {
		> 	"time_point": "@current_time",
		>	 "format": "Y/M/D HH:MM:SS"
		>	}
		>} 
		--> `@current_time` will be evaluated first then results to a dict of `{"time_point": 167343123, "format": "Y/M/D HH:MM:SS"}` as input of function call `@date_time` that might result to `2021/12/12 00:00:00`
		- Similarly, a list of `Evaluables` will be evaluated to a json-list as input for other statement. E.g:
		>```
		>{
		>	"@minus": [ "@current_time", "@field:LastEvaluated"]
		>}
		- **NOTE**: for short, result of evaluating a context can be store in a variable in parent context by using a pair of `"$" :"variable_name"`. E.g:
		>```
		>{
		> 	"evaluate plus": { 
		> 		"$": "plus_result",
		>		"@plus": [1,2]
		>	},
		>	"final_value:@plus": [1,2,3, "$plus_result"]
		>}
		--> Evaluated to: `{"evaluate plus": 3, "final_value": 9}`
		
		- **Context Types**:
			- **Basic Context**: Is simplest context just to store `variables` only
			- **Historical Context**: Is context that supports accessing the last/current snapshots of of input data
			and can `variables` in each context will be save for next evaluation.
				- **NOTE: variables precede with prefix `'.'` won't be saved in historical context for next evaluation. E.g: "$.tempVar"**
   - **Function invocation**:  Function invocation is an operation that specifies a built-in function or custom function defined by some custom modules. 
   Developers can extend functionalities of JAS by registering their own developed custom modules.
	   - a JAS built-in function looks similarly to other operations by specifying letter `@` in front of function name (E.g:`@abs`)
	   - Functions can be categorized into module and invoking it by preceding it with a module name. E.g: `@list.sort`
	   - If function is specified with a module, Translator will look-up it in registered modules, if found only-one, the one will be selected. If found zero or more than one,
	   Syntax error will be thrown.
	- **Variable property access**: an utility syntax for accessing property of a variable by specifying a path. E.g:
		>```
		> {
		>	"$object": {
		>		"key1": [ {"key2", "value2"}, {"key2", "value3"} ]
		>	},
		>	"arr": [1,2,3,4],
		>	(1)"@return": "$object[key1/1/key2]" ==> "value3"
		>	(2)"@return": "$arr[3]" ==> 4
		> }
		> ``` 
		
## Built-in operations
- **Arithmetical operators**
	- **Description:** Apply arithmetical operator on on list of Evaluables that evaluated to **_values of same type_**
	- **Syntax:** `{ "@operator": [arg1, arg2, ..., argN] }`
		- `[arg1, arg2, ..., argN]`: list of Evaluables that evaluated to json_list of same-type elements
		-  `supported types:` Number(integer/floating point) and `@plus` can be used for string concatenation as well
		- `result type`: same as first argument
	- **Operators:**
		- `@bit_and` : `a & b` -> **integer** only
		- `@bit_not`: `~a` -> **integer** only
		- `@bit_or`: `a|b` -> **integer** only
		- `@bit_xor`: `a^b` -> **integer** only
		- `@divides`: `a/b` -> **number** only
		- `@minus`: `a-b` -> **number** only
		- `@modulus`: `a%b` -> **integer** only
		- `@multiplies`: `a*b` -> **integer** only
		- `@negate`: `-b` -> **number** only, 
		- `@plus`: `a+b` -> **number/string/list/dict**
	- **Example:**
		- Valid:
			- `{"@plus":[1,2, ..., N]}` => `1 + 2 + ... + N`
			- `{"@plus":["hello", "world", "opswat client"]}` => `"hello" + "world" + "opswat client"`
			- `{"@negate":[1]}` => `-1`
		- Invalid
			- `{"@plus":[1,true]}` -> arguments with mismatched types
			-  `{"@plus":[1]}` -> missing 1 argument
- **Self-assigned Arithmetical operators**
	- **Description:** Apply arithmetical operator on a `$Variable` with another `Evaluable` and assign back the result to the `$Variable`
	- **Syntax:** `{ "@operator": ["$variable", Evaluable ] }`
		- `["$variable", Evaluable ]`: variable and the argument
		-  `supported types:` Similar as Arithmetical operators
		- `result type`: same as first argument
	- **Operators:**
		- `@s_bit_and` : `a &= b` -> **integer** only
		- `@s_bit_or`: `a|=b` -> **integer** only
		- `@s_bit_xor`: `a^=b` -> **integer** only
		- `@s_divides`: `a/=b` -> **number** only
		- `@s_minus`: `a-=b` -> **number** only
		- `@s_modulus`: `a%=b` -> **integer** only
		- `@s_multiplies`: `a*=b` -> **integer** only
		- `@s_plus`: `a+=b` -> **number/string/list/dict**
	- **Example:**
		- `{"$sum": 2,"@s_plus":["$sum", 1]}` => `set($sum=0).then($sum += 1)` ==> 3
			
- **Logical operators**: 
	- **Description:** Apply logical operator list/list of `Evaluables` that evaluated to boolean/number values
	- **Syntax:** `{ "@operator": [arg1, arg2, ..., argN] }`
		- `[arg1, arg2, ..., argN]`: list of Evaluables that evaluated to json_list of same-type of `integer/boolean`
		- `supported types:` boolean/number 
		- `result type:` bool 
	- **Operators**:
		- `@and` : `a && b` 
		- `@or`: `a || b` 
		- `@not`: `!b` 
		
	- **Example:**
		- Valid:
			- `{"@and":[true, true, false]}` => `true && true && false`==> false
			- `{"@or":[1, 2]}` => `1 || 2`==> 1
		- Invalid
			-  `{"@and":[1]}` -> missing 1 argument
			-  `{"@and":[1, true]}` -> type mismatched
- **Comparison operators**:
	- **Description:** Apply logical operator list/list of `exactly two Evaluables` that evaluated to `Consumable` value
	- **Syntax:** `{ "@operator": [arg1, arg2] }`
		- `[arg1, arg2]`: list of Evaluables that evaluated to json_list of **exactly two** same-type elements 
		- `supported types`: `Consumable` 
		- `result type`: `boolean`
	- **Operators**:
		- `@eq` : `a == b` 
		- `@neq` : `a != b` 
		- `@gt`: `a>b` 
		- `@ge`: `a>=b` 
		- `@lt`: `a<b` 
		- `@le`: `a<=b` 
	- **Example:**
		- Valid:
			- `{"@eq":[1, 1]}` => `1 == 1`==> true
			- `{"@gt":["abc", "abcd"]}` => `"abc" > "abcd"` ==> false
		- Invalid
			-  `{"@eq":[1]}` -> missing 1 argument
			-  `{"@eq":[1, "str"]}` -> type mismatched
			-  `{"@eq":[1, 2,4] }`: -> more than 2 args
- **List algorithms:**
	- **Description:** Checks that a criteria/condition is satisfied on [one/all] item(s) of a given  list/list or not
	- **Syntax:** `{"@algorithm_name":{"@cond/op": boolean, "@list": list}}`
		- `@cond`: `[Evaluable]`that must be evaluated as **boolean**
		- `@op`: `[Evaluable]` used for `@transform` operation for more meaning, can be used interchangablely with `@cond`, 
		- `@list`: `[Evaluable]` must be evaluated to list.
		- **NOTE:** in case the `@list` is not specified, then the list will be data of current context. 
		if current context data is not evaluated to a `List` then Exception will be thrown.
	- **algorithms**: 
			- `@any_of` - `[return:boolean]`: evaluated to `true` if `@cond`is evaluated to `true`on any item of given list
			- `@all_of` - `[return:boolean]`: evaluated to `true` if `@cond`is evaluated to `true`on all items of given list
			- `@none_of` - `[return:boolean]`: evaluated to `true` if `@cond`is evaluated to `false`on all items of given list
			- `@count_if` - `[return:integer]`: evaluated to number of items that satisfies `@cond`
			- `@filter_if` - `[return:list]`: pick the items that satisfy `@cond`
			- `@transform` - `[return:list]`: transforms each item in the given list to different form by Evaluable `@op`
	- **Example:**
		>```
		> {
		>  "@any_of": {
		>    "@list": "@field:anti-malware",
		>    "@cond": {
		>      "@eq": [ "@field:rtp_enabled",  1 ]
		>   }
		>  }
		>}
	    > `"@size_of: [1,2,3]"` => 3
	    > `{"@count_if":{ "@list": [1,2,3,4], "@cond:@gt": [ "@field", 2] }}` => 2
		>```		
- **Variable property access**:
	Access a variable's property via syntax `$variable[/path/to/value]`. Variable can be `Dict` or `List`
	- **Example:**
		>```
		> 1. Access Dict by path
		> {
		>  "$student_info": {
		>    "name": "myname",
		>    "school": {
		>      "name": "the school",
		>	   "addr": "treet.1 no.1"
		>   }
		>  },
		>  "school_address": "$student_info[school/addr]"
		> }  	=> { "school_address": "treet.1 no.1"}
		> 
	    > 2. Access List by index
	    >  {
		>	  "$arr": [1,2,3,4],
		>	  "item4": "$arr[3]"
		>	} => {"item4": 4}
		>
		> 3. Query by jpath(combine both index and path)
		>  {
		>	  "$object": {
		>		"arr": [
		>		  {
		>			"name": "item1"
		>		  },
		>		  {
		>			"name": "item2"
		>		  },
		>		  {
		>			"name": "item3"
		>		  }
		>		]
		>	  },
		>	  "myquery": "$object[arr/2/name]"
		>	} 				=> {"myquery": "item3"}
		>
		> 4. Query by jpath with $variable expansion
		>  {
		> 	  "$field_arr": "arr",
		> 	  "$item3_idx": 2,
		>	  "$object": {
		>		"arr": [
		>		  {
		>			"name": "item1"
		>		  },
		>		  {
		>			"name": "item2"
		>		  },
		>		  {
		>			"name": "item3"
		>		  }
		>		]
		>	  },
		>	  "myquery": "$object[$field_arr/$item3_idx/name]"
		>	} 				=> {"myquery": "item3"}
		>
		>```		
- **List.Operations:**
	Built-in operations for manipulating list type
	- `@list.append`: `{ "$arr": [1,2,3], "@list.append": ["$arr", 4,5,6]}` => [1,2,3,4,5,6] 
	- `@list.count`: `{ "$arr": [1,2,3,2,4,2], "@list.count": ["$arr", 2}` => 3
	- `@list.extend`: `{ "$arr": [1,2,3], "$arr2": [4,5,6], "@list.extend": ["$arr", "$arr2"]}` => [1,2,3,4,5,6]
	- `@list.insert`: `{ "$arr": [1,2,3], "@list.insert": ["$arr", 1, 10}` => [1,10,2,3]
	- `@list.len`: `{"@list.len":[1,2,3,4]}` => 4
	- `@list.pop`: `{ "$arr": [1,2,3], "@list.pop": ["$arr", 1]}` => [1,3] // pop out the element at index 1
	- `@list.remove`: `{ "$arr": [1,2,5,3], "@list.remove": ["$arr", 5]}` => [1,2,3] // remove the value 5
	- `@list.size`: similar to `list.len`
	- `@dict.contains`: `{ "$arr": [1,2,3], "@list.contains": ["$arr", 3]}` => true // check existence of a value in list
	- `@list.clear`: `{"@list.clear": [1,2,3]}` => [] // empty the list
	- `@list.is_empty`: `{"@list.empty": []}` => true // check a list is empty or not
	- `@list.sort`: `{"@list.sort": [3,2,1]}` => [1,2,3]
	- `@list.unique`: `{"@list.unique": [1,1,2,2,3,4]}` => [1,2,3] // **NOTE**: applied on sorted list only 
	then for sure, it should be: `{"@unique:@sort": [1,1,2,2,3,4]}`
	
- ** Dict.Operations:**
	Built-in operations for manipulating dictionary type
	- `@dict.clear`: simirlar to `list.clear`
	- `@dict.contains`: `{"$object": {"address": "here"}, "@dict.contains": ["$object", "address"]}` => true 
	- `@dict.erase`: `{"$object": {"address": "here"}, "@dict.erase": ["$object", "address"]}` => `{}` 
	- `@dict.exists`: `{"$object": {"key1": {"key2": "value"}}, "@dict.exists": ["$object", "key1/key2"]}` => true 
	- `@dict.get`: `{"$object": {"address": "here"}, "@dict.get": ["$object", "address"]}` => `"here"`
	- `@dict.get_path`: `{"$object": {"key1": {"key2": "value"}}, "@dict.get_path": ["$object", "key1/key2"]}` => "value" 
	- `@dict.is_empty`: similar to `list.empty`
	- `@dict.keys`: `{"$object": {"address": "here", "name": "dev"}, "@dict.keys": "$object"}` => ["address", "name"]
	- `@dict.size`: `{"$object": {"address": "here", "name": "dev"}, "@dict.size": "$object"}` => 2
	- `@dict.update`: `{"$object": {"address": "here", "name": "dev"}, "@dict.size": ["$object", {"age": 100}]}` => `{"address": "here", "name": "dev", "age": 100}`
	- `@dict.values`: `{"$object": {"address": "here", "name": "dev"}, "@dict.values": "$object"}` => ["here", "dev"]
	
- **Globl Functions**:
	- `@abs`:  `(->number)` math.abs(-1) -> 
	- `@current_time`:  `(->integer)` get unix-time timestamp of system_clock 
	- `@current_time_diff:CERTAIN_TIME_POIN`:  `(->integer)` return the diff between current time with `CERTAIN_TIME_POIN`  
	- `@tolower: string`: `(->string)`converts a string to lower case
	- `@tolupper: string`: `(->string)` converts a string to upper case
	- `@prop: string` : `(->json)`gets value of an assigned property - same as "#property_name"
	- `@cmp_ver: ["v1", "v2"]`:  `(->integer)`compare the strings as versions(`-1: v1 < v2, 0: equal, 1: v1>v2`)
	- `@eq_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 == v2`
	- `@ne_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 != v2`
	- `@gt_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 > v2`
	- `@lt_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 < v2`
	- `@ge_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 >= v2`
	- `@le_ver: ["v1", "v2"]`:  `(->integer)` true if `v1 <= v2`
	- `@contains: ["s1", "s2"]`:`(->boolean)` true if `s2` appears in `s1`
	- `@unix_timestamp: string`:  `(->integer)`converts a string that represents a date_time to unix 
	- `@is_even:Integer`: check a number is even or not
	- `@is_odd:Integer`: check a number is odd or not
	- `@match_ver:["someversion", "x-pattern"]`: check a `version` match an  x-pattern or not. Eg: `1.1` will match `1.x`, 
	`@match_ver:["1.2.3", ["1.x", "2.x", 3.3.x]]` => true
	time_stamp
	- `@has_null_val: list/object`:`(->boolean)` true if given list/object contains any null value
	- `... add more ...`
	
- **Context data maniputation functions**: Bellow functions are for accessing/manipulating historical input data 
	> - `@hfield`: select the data of field that contains both last snapshot and current snapshot, return of this function must not be consume dirrectly by evaluable operations
	>	- input: 
	>		- `[string]`: path to the field. E.g: `/Antivirus/product/name`
	>		- `[JsonObject]: {"path": "path/to/field", "iid": "item_id"}`, for field that points to a list only
	>	- return: `JsonObject`
	> - `@hfield2arr`: shorthand for `@hfield`, for case field points to an list
	>	- input:  `["path/to/field", "iid": "item_id"]` ->  list contains 2 elements
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
	> - `@snchg:path/to/field` `(->boolean)` return true if last/cur data snapshot in current context are different
	> - `@evchg:variable_name` `(->boolean)` return true if last/cur value of `variable_name` in same context are different
	> - `@last_eval:variable_name` `(->Consumable)` return last value of varirable `variable_name`
	>```


	