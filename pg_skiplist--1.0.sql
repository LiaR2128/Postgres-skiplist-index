CREATE FUNCTION skiplist_init() RETURNS text
AS 'MODULE_PATHNAME', 'pg_skiplist_init'
LANGUAGE C STRICT;

CREATE FUNCTION skiplist_insert(int4, int4) RETURNS bool
AS 'MODULE_PATHNAME', 'pg_skiplist_insert'
LANGUAGE C STRICT;

CREATE FUNCTION skiplist_search(int4) RETURNS int4
AS 'MODULE_PATHNAME', 'pg_skiplist_search'
LANGUAGE C STRICT;

CREATE FUNCTION skiplist_count() RETURNS int8
AS 'MODULE_PATHNAME', 'pg_skiplist_count'
LANGUAGE C STRICT;

CREATE FUNCTION skiplist_check() RETURNS bool
AS 'MODULE_PATHNAME', 'pg_skiplist_check'
LANGUAGE C STRICT;
