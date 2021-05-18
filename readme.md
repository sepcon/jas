# Json evAluable Syntax [JAS]
## Purposes
- Easy to parse evaluable syntax written in json
- A set of rules can be created and changed flexibly without modification from source code
- Can be used to evaluate some kind of json-like data then determine further actions accordingly

## Specification
###  Description: 
-	Everything is considered as an evaluable expression and finally will be evaluated to a ***consumable value(number, string, boolean)*** or an exception will be thrown if an expression cannot be evaluated(invalid syntax...)
-	Evaluable expression can be nested as parameters of other one recursively and the nested one will be evaluated first
### Details:
- **Consumable types**:  `number(floating point, integer), string, boolean`
- **Evaluable:**`<evaluable>` abstract concept of an operation or syntax or a constant value that is finally evaluated to a consumable value
- **Direct values:**`<direct_value>`
	- **Description:** Number(integer, floating point), string, bool
	- **Example:** `1, 2, 3..., 1.0, 1.1 ..., true, false, "hello world"`
- **Arithmetical operators:** `<arthm_op>`
	- **Description:** Apply arithmetical operator on each pair of Evaluables that evaluated to values of same type
	- **Supported types:** Number(integer/floating point) and `@plus` for string concatenation
	- **Params:** array of Evaluables
	- **Return:** Same type as parameter `(eval(integer+integer) = integer)`
	- **Operations:**
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
	- **Example:**
		- Valid:
			- `{"@plus":[1,2, ..., N]}` => `1 + 2 + ... + N`
			- `{"@plus":["hello", "world", "opswat client"]}` => `"hello" + "world" + "opswat client"`
			- `{"@negate":[1]}` => `-1`
		- Invalid
			- `{"@plus":[1,true]}` -> arguments with mismatched types
			-  `{"@plus":[1]}` -> missing 1 argument
- **Logical operators**: `<logical_op>`
	- **Description:** Apply logical operator on pair of Evaluables that evaluated to boolean/number values
	- **Supported types:** boolean/number 
	- **Params:** array of **exactly  2** Evaluables that evaluated to boolean/number values
	- **Return:** Same type as parameter `(eval(integer+integer) = integer)`
	- **Operations:**
		- `@and` : `a && b` 
		- `@or`: `a||b` 
		- `@not`: `!b` 
	- **Example:**
		- Valid:
			- `{"@and":[true, false]}` => `true && false`
			- `{"@or":[1, 2]}` => `1||2`
		- Invalid
			- `{"@and":[true, false, false]}` -> more than 2 arguments
			-  `{"@and":[1]}` -> missing 1 argument
			-  `{"@and":[1, true]}` -> type mismatched
- **List operations:**`<list_op>`
	- **Description:** Checks that a criteria is satisfied on a list or not
	- **Params:** 
		- `@cond`: `[evaluable][required]`that will be evaluated as **boolean**
		- `@path`: `[string]`specifies the path to list to be applied, empty if data of current context is an array
		- `@item_id`: `[string]`: specifies unique field of a list item. Use this when want to compare corresponding values between two lists. if empty, item in 2 lists will be mapped by index.
	- **Operations:**
		- `@any_of` - `[return:boolean]`: evaluated to `true` if `@cond`is evaluated to `true`on any item of given list
		- `@all_of` - `[return:boolean]`: evaluated to `true` if `@cond`is evaluated to `true`on all items of given list
		- `@none_of` - `[return:boolean]`: evaluated to `true` if `@cond`is evaluated to `false`on all items of given list
		- `@size_of` - `[return:integer]`: evaluated to size of given list
		- `@count_if` - `[return:integer]`: evaluated to number of items that satisfies `@cond`
		- **NOTE:** to check a list is empty we can specify as: `{"@any_of" : true} or {"@any_of": {"@path": "the_list", "@cond" : true}}`
	- **Example:**
		- Valid:
			> ```
			> {
			>  "@any_of": {
			>    "@path": "anti-malware",
			>    "@cond": {
			>      "@eq": [ {  "@value_of": "rtp_enabled" }, 1 ]
			>   }
			>  }
			>}
			
		- Invalid
			> ```
			> {
			>  "@any_of": {
			>    "@path": "anti-malware",
			>    "@cond": {
			>      "@plus": [ {  "@value_of": "rtp_enabled" }, 1 ] 
			>   }
			>  }
			>}
			> -> why: plus (1,  value_of("rtp_enabled") will evaluate to integer instead of boolean  
	 
-  **Context value extractor**:
	- **Description:**  gets a value of given path from current context
		- `Context`: contains data that an evaluable expression is being applied on. E.g: when apply a criteria `@cond` on a list item. each item's data will be consider as current context.
	- **Params:** 
		- `@name`: `[string]`  specifies key/path name of the value want to get - if **empty**, the value is extracted dirrectly from current-context
		- `@default`: default value if given name/path does not exist
		- `@snapshot`: `[string]`specifies the previous/current context (0: previous, 1: current). with SOH report we can consider old/new snapshot as data on old/new report, respectively.
	- **Return:** `[direct_value]`
	- **Operations:**
		- `@value_of` 
	- **Example:**
		- `{"@value_of": "rtp_enabled"}`
		- `{"@value_of": {"@name" : "rtp_enabled", "@snapshot" : 1, "@default": 0}}`
		- `{"@value_of": {"@snapshot" : 1, "@default": 0}}` -> invalid: missing `@name`
- **Function invoker**:
	- **Description:**  invokes a function from a predefined list of client
	- **Params:** 
		- `@name`: `[string]` specifies name of function invoke - **required** if expression is specified as json object
		- `@param`: `[evaluable]` an evaluable syntax that will be evaluated before passing to function
	- **Return:** `[direct_value]`
	- **Operations:**
		- `@call` 
	- **Example:**
		- `{"@call": "current_time"}`
		- `{"@call": {"@func" : "tolower", "param" : "HELLO"}}`
	- **Predefined functions**:
		> - `current_time <integer()>`:  that represents current system unix-time 
		> - `tolower: <string(string)>` converts a string to lower case
		> - `tolupper: <string(string)>` converts a string to upper case
		> - `snapshot_changed: <boolean()>` returns true if old/new snapshot in current context are different
		> - `get_prop: <evaluated(string)>` returns a property specified by `name`. E.g: `{ "@call" : { "@func": "get_prop" , "param": "last_report_time"}}`
		> - `... add more ...`

	