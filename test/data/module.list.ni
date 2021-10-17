{ "@list.append":[ [1,2,3], 4,5,6 ]}
[1,2,3,4,5,6]
{ "@list.append":[ [1,2,3] ]}
{"@exception":"InvalidArgument"}
{ "$mylist": [1,2,3], "@list.append":[ "$mylist", 4,5,6 ]}
[1,2,3,4,5,6]
{ "$mylist": [1,2,3], "removed:@list.remove":[ "$mylist", 1 ], "final": "$mylist"}
{"removed": true, "final": [2, 3]}
{ "$mylist": [1,2,3], "@list.extend":[ "$mylist", [4,5], 6 ]}
[1,2,3,4,5,6]
{ "$mylist": [1,2,3], "@list.extend":[ "$mylist", [4,5], [6,7] ]}
[1,2,3,4,5,6,7]
{ "$mylist": [1,2,3], "@list.insert":[ "$mylist", 1, 6 ]}
[1,6,2,3]
{ "$mylist": [1,2,3], "$good:@list.insert":[ "$mylist", 3, 6 ], "@return":"$mylist"}
[1,2,3,6]
{ "$mylist": [1,2,3], "$good:@list.insert":[ "$mylist", 7, 6 ], "@return":"$mylist"}
{"@exception":"InvalidArgument"}
{ "$mylist": [1,2,3], "$good:@list.insert":[ "$mylist", 4, 6 ], "@return":"$mylist"}
{"@exception":"InvalidArgument"}
{ "$mylist": [1,2,3], "@list.len": "$mylist"}
3
{"@list.len":[1,2,3]}
3
{ "$mylist": [3,2,3,1], "@list.sort": "$mylist"}
[1,2,3,3]
{ "$mylist": [3,2,3,1], "@list.unique:@list.sort": "$mylist"}
[1,2,3]
//THIS for making sure modifying a list won't impact the value of other list that refer to it before
{"$mylist": [3,2,3,1], "$otherlist": "$mylist", "mylist:@list.unique:@list.sort": "$mylist", "otherlist":"$otherlist"}
{"mylist": [1, 2, 3], "otherlist": [3, 2, 3, 1]}
{"@list.count": ["$thelist", 1], "$thelist":[1,1,2,2,3,3,1]}
3
{"@list.count": ["$thelist", 4], "$thelist":[1,1,2,2,3,3,1]}
0
{"@list.count": [ [1,1,2,2,3,3,1]  ]}
{"@exception":"InvalidArgument"}
{"@plus":[[1],[2]]}
[1,2]
{"$arr": [1], "@s_plus":["$arr",[2]]}
[1,2]
{"@eq":[[1,2], [1,2]]}
true
{"@eq":[[1,2], [1,3]]}
false
{"@lt":[[1,2], [1,3]]}
true
//Test invocation chain
{"$arr1":[2,2,1],"$arr2":[1,2],"@eq":["@list.unique:@list.sort:$arr1","@list.sort:$arr2"]}
true
{"$arr1":[1,0],"$arr2":"$arr1","$sorted:@list.sort":"$arr1","@eq":["$arr2","$arr1"]}
false
{"@list.pop":[[1,2,3], 0]}
1
{"@list.pop":[[1,2,3], 2]}
3
{"@list.pop":[[1,2,3], 5]}
{"@exception":"InvalidArgument"}
{"@list.clear": [1,2,3]}
[]
{"@list.is_empty": [1,2,3]}
false
{"@list.is_empty": []}
true
{"@list.contains": [[1,2,3], 4]}
false
{"@list.contains": [[1,2,3], 3]}
true
{"@list.contains": [[1,2,3], "3"]}
false
