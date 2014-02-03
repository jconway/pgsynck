/*
 * pg_syntax_checker
 *
 * Run SQL text through the PostgreSQL parser and return syntax error
 * information for each contained statement.
 * Joe Conway <mail@joeconway.com>
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written agreement
 * is hereby granted, provided that the above copyright notice and this
 * paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
 * DOCUMENTATION, EVEN IF THE AUTHOR OR DISTRIBUTORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUTHOR AND DISTRIBUTORS HAS NO OBLIGATIONS TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "parser/parser.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

#define OUTPUT_COLS	4

typedef enum
{
	NOTINAQUOTE,			/* outside any quotes */
	SINGLEQUOTE,			/* 'text' */
	DOUBLEQUOTE,			/* "text" */
	DOLLARQUOTE				/* $$text$$ */
}	quotetype;

/*
 * exported functions
 */
extern Datum pg_syntax_checker(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pg_syntax_checker);
Datum
pg_syntax_checker(PG_FUNCTION_ARGS)
{
/*	List			   *raw_parsetree_list = NULL; */
	char			   *query_string = NULL;
	ErrorData		   *edata = NULL;
	MemoryContext		oldcontext = CurrentMemoryContext;
	MemoryContext		per_query_ctx;
	ReturnSetInfo	   *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	TupleDesc			tupdesc;
	Tuplestorestate	   *tupstore;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not " \
						"allowed in this context")));

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	query_string = text_to_cstring(PG_GETARG_TEXT_PP(0));

	{
		int			j = 0;
		Datum		values[OUTPUT_COLS];
		bool		nulls[OUTPUT_COLS];

		memset(values, 0, sizeof(values));
		memset(nulls, 0, sizeof(nulls));

		PG_TRY();
		{
			raw_parser(query_string);

			/* cursorpos */
			values[j++] = Int32GetDatum(0);

			/* sqlerrcode */
			values[j++] = Int32GetDatum(0);

			/* message - primary error message */
			values[j++] = CStringGetTextDatum("");

			/* hint - hint message */
			values[j++] = CStringGetTextDatum("");

			tuplestore_putvalues(tupstore, tupdesc, values, nulls);
		}
		PG_CATCH();
		{
			/* Save error info */
			MemoryContextSwitchTo(oldcontext);
			edata = CopyErrorData();
			FlushErrorState();

			/* cursorpos - cursor index into query string */
			values[j++] = Int32GetDatum(edata->cursorpos);

			/* sqlerrcode - encoded ERRSTATE */
			values[j++] = Int32GetDatum(edata->sqlerrcode);

			/* message - primary error message */
			values[j++] = CStringGetTextDatum(edata->message ? edata->message:"");

			/* hint - hint message */
			values[j++] = CStringGetTextDatum(edata->hint ? edata->hint:"");

			tuplestore_putvalues(tupstore, tupdesc, values, nulls);
			FreeErrorData(edata);
		}
		PG_END_TRY();
	}

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

