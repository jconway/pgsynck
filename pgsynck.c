/*
 * pgsynck
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

#define PGSYNCK_COLS	5

static char *get_one_query(char **q);

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
extern Datum pgsynck(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pgsynck);
Datum
pgsynck(PG_FUNCTION_ARGS)
{
/*	List			   *raw_parsetree_list = NULL; */
	char			   *query_string = NULL;
	char			   *oneq = NULL;
	char			   *q;
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

	q = query_string = text_to_cstring(PG_GETARG_TEXT_PP(0));
	oneq = get_one_query(&q);

	while (oneq != NULL)
	{
		int			j = 0;
		Datum		values[PGSYNCK_COLS];
		bool		nulls[PGSYNCK_COLS];

		memset(values, 0, sizeof(values));
		memset(nulls, 0, sizeof(nulls));

		/* sql */
		values[j++] = CStringGetTextDatum(oneq);

		PG_TRY();
		{
			raw_parser(oneq);

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

		oneq = get_one_query(&q);
	}

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

static char *
get_one_query(char **q)
{
	char	   *retstr = NULL;
	char	   *p;
	quotetype	qtype = NOTINAQUOTE;
	bool		in_quote = false;
	bool		in_comment = false;
	char	   *dolq = NULL;
	char	   *dolq_startp = NULL;
	bool		dolq_started = false;
	bool		sl_comment = false;
	bool		ml_comment = false;

	/* skip over any leading whitespace */
	while (isspace(**q))
		(*q)++;

	/* fast path if we are already at the end of input */
	if (**q == 0)
		return NULL;

	p = *q;
	for(;;)
	{
		/* single quote */
		if (!in_comment && **q == '\'')
		{
			if (!in_quote)
			{
				qtype = SINGLEQUOTE;
				in_quote = true;
			}
			else if (qtype == SINGLEQUOTE)
			{
				qtype = NOTINAQUOTE;
				in_quote = false;
			}
			/* ignore single quote if already in another type of quote */
		}

		/* double quote */
		if (!in_comment && **q == '\"')
		{
			if (!in_quote)
			{
				qtype = DOUBLEQUOTE;
				in_quote = true;
			}
			else if (qtype == DOUBLEQUOTE)
			{
				qtype = NOTINAQUOTE;
				in_quote = false;
			}
			/* ignore double quote if already in another type of quote */
		}

		/* dollar quote */
		if (!in_comment && **q == '$')
		{
			if (!in_quote)
			{
				if (!dolq_started)
				{
					dolq_startp = *q;
					dolq_started = true;
				}
				else
				{
					int		dolq_len = *q - dolq_startp;

					dolq = palloc0(dolq_len + 1);
					memcpy(dolq, dolq_startp, dolq_len);

					qtype = DOLLARQUOTE;
					in_quote = true;
				}
			}
			else if (qtype == DOLLARQUOTE)
			{
				int		dolq_len = strlen(dolq);

				if (strncmp(*q, dolq, dolq_len) == 0)
				{
					/* skip ahead */
					*q += dolq_len;
					qtype = NOTINAQUOTE;
					in_quote = false;
				}
			}
			/* ignore dollar quote if already in another type of quote */
		}

		/* are we starting a multiline comment */
		if (!in_comment && !in_quote && **q == '/' && *(*q + 1) == '*')
		{
			/* skip ahead */
			(*q)++;
			ml_comment = true;
			in_comment = true;
		}

		/* are we starting a single line comment */
		if (!in_comment && !in_quote && **q == '-' && *(*q + 1) == '-')
		{
			/* skip ahead */
			(*q)++;
			sl_comment = true;
			in_comment = true;
		}

		/* are we ending a multiline comment */
		if (in_comment && ml_comment && **q == '*' && *(*q + 1) == '/')
		{
			/* skip ahead */
			(*q)++;
			ml_comment = in_comment = false;
		}

		/* are we ending a single line comment */
		if (in_comment && sl_comment && **q == '\n')
		{
			sl_comment = in_comment = false;
		}

		if ((!in_quote && !in_comment && **q == ';') || **q == 0)
		{
			size_t	retstrlen = *q - p;

			if (retstrlen != 0)
			{
				retstr = palloc0(retstrlen + 1);
				memcpy(retstr, p, retstrlen);
			}
			break;
		}

		(*q)++;
	}

	if (**q == ';')
		(*q)++;

	return retstr;
}
