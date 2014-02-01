pgsynck
=======

Run SQL text through the PostgreSQL parser and return syntax error information for each contained statement.

Usage example:
--------------
test=# select * from pgsynck
test-# ($$
test$#     select '1;2;3/* not a comment */' from "My;Table" as from;
test$#     select x from (select 1 as x);
test$#     /* test;test */ begin;
test$#     insert into foo2 values ('1''2','3;4');
test$#     abort my trans
test$# $$);
-[ RECORD 1 ]-------------------------------------------------------------
sql        | 
           |     select '1;2;3/* not a comment */' from "My;Table" as from
cursorpos  | 59
sqlerrcode | 16801924
message    | syntax error at or near "from"
hint       | 
-[ RECORD 2 ]-------------------------------------------------------------
sql        | 
           |     select x from (select 1 as x)
cursorpos  | 20
sqlerrcode | 16801924
message    | subquery in FROM must have an alias
hint       | For example, FROM (SELECT ...) [AS] foo.
-[ RECORD 3 ]-------------------------------------------------------------
sql        | 
           |     /* test;test */ begin
cursorpos  | 0
sqlerrcode | 0
message    | 
hint       | 
-[ RECORD 4 ]-------------------------------------------------------------
sql        | 
           |     insert into foo2 values ('1''2','3;4')
cursorpos  | 0
sqlerrcode | 0
message    | 
hint       | 
-[ RECORD 5 ]-------------------------------------------------------------
sql        | 
           |     abort my trans
           | 
cursorpos  | 12
sqlerrcode | 16801924
message    | syntax error at or near "my"
hint       | 

TODO:
--------------
1. If last statement ends in semicolon, an extra empty SQL string is checked. Skip that empty string
