#include <sys/stat.h>

#include "postgres.h"
#include "fmgr.h"

#include "access/reloptions.h"
#include "catalog/pg_foreign_table.h"
#include "commands/defrem.h"
#include "foreign/fdwapi.h"
#include "optimizer/planmain.h"
#include "utils/elog.h"


PG_MODULE_MAGIC;


/* FDW routines */
extern void parquetGetForeignRelSize(PlannerInfo *root,
					  RelOptInfo *baserel,
					  Oid foreigntableid);
extern void parquetGetForeignPaths(PlannerInfo *root,
					RelOptInfo *baserel,
					Oid foreigntableid);
extern ForeignScan *parquetGetForeignPlan(PlannerInfo *root,
                      RelOptInfo *baserel,
                      Oid foreigntableid,
                      ForeignPath *best_path,
                      List *tlist,
                      List *scan_clauses,
                      Plan *outer_plan);
extern TupleTableSlot *parquetIterateForeignScan(ForeignScanState *node);
extern void parquetBeginForeignScan(ForeignScanState *node, int eflags);
extern void parquetEndForeignScan(ForeignScanState *node);
extern void parquetReScanForeignScan(ForeignScanState *node);

PG_FUNCTION_INFO_V1(parquet_fdw_handler);
Datum
parquet_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdwroutine = makeNode(FdwRoutine);

	fdwroutine->GetForeignRelSize = parquetGetForeignRelSize;
	fdwroutine->GetForeignPaths = parquetGetForeignPaths;
	fdwroutine->GetForeignPlan = parquetGetForeignPlan;
	fdwroutine->BeginForeignScan = parquetBeginForeignScan;
	fdwroutine->IterateForeignScan = parquetIterateForeignScan;
	fdwroutine->ReScanForeignScan = parquetReScanForeignScan;
	fdwroutine->EndForeignScan = parquetEndForeignScan;

	PG_RETURN_POINTER(fdwroutine);
}

PG_FUNCTION_INFO_V1(parquet_fdw_validator);
Datum
parquet_fdw_validator(PG_FUNCTION_ARGS)
{
    List       *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
    Oid         catalog = PG_GETARG_OID(1);
    ListCell   *lc;
    bool        filename_provided = false;

    /* Only check table options */
    if (catalog != ForeignTableRelationId)
        PG_RETURN_VOID();

    foreach(lc, options_list)
    {
        DefElem    *def = (DefElem *) lfirst(lc);

        if (strcmp(def->defname, "filename") == 0)
        {
            struct stat stat_buf;

            if (stat(defGetString(def), &stat_buf) != 0)
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                         errmsg("parquet_fdw: file \"%s\" does not exist",
                                defGetString(def))));
            filename_provided = true;
        }
        else if (strcmp(def->defname, "sorted") == 0)
            ;  /* do nothing */
        else
        {
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                     errmsg("parquet_fdw: invalid option \"%s\"",
                            def->defname)));
        }
    }

    if (!filename_provided)
        elog(ERROR, "parquet_fdw: filename is required");

    PG_RETURN_VOID();
}

