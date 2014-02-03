/* contrib/pg_syntax_checker/pg_syntax_checker--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_syntax_checker" to load this file. \quit

CREATE FUNCTION pg_syntax_checker
(
  IN  sql text,
  OUT cursorpos integer,
  OUT sqlerrcode integer,
  OUT message text,
  OUT hint text
)
RETURNS SETOF RECORD
AS 'MODULE_PATHNAME','pg_syntax_checker'
LANGUAGE C STRICT;

