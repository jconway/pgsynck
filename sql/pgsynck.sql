create extension pgsynck;

select * from pgsynck
($$
    select '1;2;3/* not a comment */' from "My;Table" as from;
    select x from (select 1 as x);
    /* test;test */ begin;
    insert into foo2 values ('1''2','3;4');
    abort my trans
$$);

select * from pgsynck
($$
    --1
    select 2;
    select a from -- b; -- select 1;
    c
$$);

select * from pgsynck
($$
    
    select 2;
    
$$);
