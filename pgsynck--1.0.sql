/* contrib/pgsynck/pgsynck--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgsynck" to load this file. \quit

CREATE FUNCTION pgsynck
(
  INOUT sql text,
  OUT cursorpos integer,
  OUT sqlerrcode integer,
  OUT message text,
  OUT hint text
)
RETURNS SETOF RECORD
AS 'MODULE_PATHNAME','pgsynck'
LANGUAGE C STRICT;

