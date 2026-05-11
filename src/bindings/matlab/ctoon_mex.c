/**
 * @file ctoon_mex.c
 * @brief Internal MEX gateway for CToon — do NOT call directly from MATLAB.
 *
 * Use the public wrappers instead:
 *   ctoon_encode(value)          encode MATLAB value → TOON string
 *   ctoon_decode(str)            decode TOON string  → MATLAB value
 *   ctoon_read(filepath)         read   .toon file   → MATLAB value
 *   ctoon_write(value, path)     write  .toon file
 *
 * Dispatch key (prhs[0], char):
 *   "encode"   prhs[1]=value              → plhs[0]=toon_str
 *   "decode"   prhs[1]=toon_str           → plhs[0]=value
 *   "read"     prhs[1]=filepath           → plhs[0]=value
 *   "write"    prhs[1]=value, prhs[2]=path
 *   "version"                             → plhs[0]=ver_str
 *
 * MATLAB ↔ TOON type mapping:
 *   []            ↔  null
 *   logical       ↔  bool
 *   double scalar ↔  real
 *   int64  scalar ↔  sint
 *   uint64 scalar ↔  uint
 *   char/string   ↔  str
 *   cell array    ↔  array
 *   struct        ↔  object
 */

#include "mex.h"
#include "ctoon.h"

#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * Forward declarations
 * ========================================================================= */

static mxArray      *val_to_mx(ctoon_val *val);
static ctoon_mut_val *mx_to_mut(ctoon_mut_doc *doc, const mxArray *mx);

/* =========================================================================
 * ctoon_val → mxArray
 * ========================================================================= */

static mxArray *val_to_mx(ctoon_val *val) {
    if (!val || ctoon_is_null(val))
        return mxCreateDoubleMatrix(0, 0, mxREAL);

    if (ctoon_is_true(val))  return mxCreateLogicalScalar(1);
    if (ctoon_is_false(val)) return mxCreateLogicalScalar(0);

    if (ctoon_is_uint(val)) {
        mxArray *out = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL);
        *((uint64_t *)mxGetData(out)) = ctoon_get_uint(val);
        return out;
    }

    if (ctoon_is_sint(val)) {
        mxArray *out = mxCreateNumericMatrix(1, 1, mxINT64_CLASS, mxREAL);
        *((int64_t *)mxGetData(out)) = ctoon_get_sint(val);
        return out;
    }

    if (ctoon_is_real(val))
        return mxCreateDoubleScalar(ctoon_get_real(val));

    if (ctoon_is_str(val)) {
        const char *s = ctoon_get_str(val);
        return mxCreateString(s ? s : "");
    }

    if (ctoon_is_arr(val)) {
        size_t n = ctoon_arr_size(val);
        mxArray *cell = mxCreateCellMatrix((mwSize)n, 1);
        for (size_t i = 0; i < n; i++)
            mxSetCell(cell, (mwIndex)i, val_to_mx(ctoon_arr_get(val, i)));
        return cell;
    }

    if (ctoon_is_obj(val)) {
        size_t sz = ctoon_obj_size(val);
        if (sz == 0)
            return mxCreateStructMatrix(1, 1, 0, NULL);

        const char **keys = (const char **)mxMalloc(sz * sizeof(const char *));
        if (!keys)
            mexErrMsgIdAndTxt("ctoon:memory", "mxMalloc failed for field names.");

        ctoon_obj_iter it = ctoon_obj_iter_with(val);
        ctoon_val *kv;
        size_t ki = 0;
        while ((kv = ctoon_obj_iter_next(&it)) != NULL)
            keys[ki++] = ctoon_get_str(kv);

        mxArray *st = mxCreateStructMatrix(1, 1, (int)sz, keys);
        mxFree((void *)keys);

        it = ctoon_obj_iter_with(val);
        ki = 0;
        while ((kv = ctoon_obj_iter_next(&it)) != NULL) {
            mxSetFieldByNumber(st, 0, (int)ki,
                               val_to_mx(ctoon_obj_iter_get_val(kv)));
            ki++;
        }
        return st;
    }

    return mxCreateDoubleMatrix(0, 0, mxREAL);
}

/* =========================================================================
 * mxArray → ctoon_mut_val
 * ========================================================================= */

static ctoon_mut_val *mx_to_mut(ctoon_mut_doc *doc, const mxArray *mx) {
    if (!mx || mxIsEmpty(mx))
        return ctoon_mut_null(doc);

    if (mxIsLogical(mx) && mxIsScalar(mx))
        return ctoon_mut_bool(doc, ((mxLogical *)mxGetData(mx))[0] != 0);

    if (mxIsChar(mx)) {
        char *s = mxArrayToString(mx);
        ctoon_mut_val *v = ctoon_mut_str(doc, s);
        mxFree(s);
        return v;
    }

    if (mxIsDouble(mx) && mxIsScalar(mx))
        return ctoon_mut_real(doc, mxGetScalar(mx));

    if (mxIsInt64(mx) && mxIsScalar(mx))
        return ctoon_mut_sint(doc, *((int64_t *)mxGetData(mx)));

    if (mxIsUint64(mx) && mxIsScalar(mx))
        return ctoon_mut_uint(doc, *((uint64_t *)mxGetData(mx)));

    if (mxIsDouble(mx)) {
        size_t n = mxGetNumberOfElements(mx);
        double *pr = (double *)mxGetPr(mx);
        ctoon_mut_val *arr = ctoon_mut_arr(doc);
        for (size_t i = 0; i < n; i++)
            ctoon_mut_arr_append(arr, ctoon_mut_real(doc, pr[i]));
        return arr;
    }

    if (mxIsCell(mx)) {
        size_t n = mxGetNumberOfElements(mx);
        ctoon_mut_val *arr = ctoon_mut_arr(doc);
        for (size_t i = 0; i < n; i++)
            ctoon_mut_arr_append(arr,
                mx_to_mut(doc, mxGetCell(mx, (mwIndex)i)));
        return arr;
    }

    if (mxIsStruct(mx)) {
        int nf = mxGetNumberOfFields(mx);
        ctoon_mut_val *obj = ctoon_mut_obj(doc);
        for (int fi = 0; fi < nf; fi++) {
            const char *fname = mxGetFieldNameByNumber(mx, fi);
            ctoon_mut_obj_put(obj,
                ctoon_mut_str(doc, fname),
                mx_to_mut(doc, mxGetFieldByNumber(mx, 0, fi)));
        }
        return obj;
    }

    mexWarnMsgIdAndTxt("ctoon:unsupportedType",
        "Unsupported MATLAB type '%s' — serialised as null.",
        mxGetClassName(mx));
    return ctoon_mut_null(doc);
}

/* =========================================================================
 * Helpers
 * ========================================================================= */

static char *require_str(const mxArray *mx, int argn) {
    if (!mxIsChar(mx))
        mexErrMsgIdAndTxt("ctoon:badArg",
            "Argument %d must be a character string.", argn);
    char *s = mxArrayToString(mx);
    if (!s)
        mexErrMsgIdAndTxt("ctoon:memory",
            "mxArrayToString failed (arg %d).", argn);
    return s;
}

static ctoon_mut_doc *doc_from_mx(const mxArray *mx) {
    ctoon_mut_doc *doc = ctoon_mut_doc_new(NULL);
    if (!doc)
        mexErrMsgIdAndTxt("ctoon:memory", "ctoon_mut_doc_new() failed.");
    ctoon_mut_doc_set_root(doc, mx_to_mut(doc, mx));
    return doc;
}

/* =========================================================================
 * Dispatch handlers
 * ========================================================================= */

static void do_encode(int nlhs, mxArray *plhs[],
                      int nrhs, const mxArray *prhs[]) {
    if (nrhs < 2)
        mexErrMsgIdAndTxt("ctoon:badArg", "encode: value argument required.");
    ctoon_mut_doc *doc = doc_from_mx(prhs[1]);
    size_t len = 0;
    char *out = ctoon_mut_write(doc, &len);
    ctoon_mut_doc_free(doc);
    if (!out)
        mexErrMsgIdAndTxt("ctoon:encodeError", "ctoon_mut_write() failed.");
    if (nlhs > 0) plhs[0] = mxCreateString(out);
    free(out);
}

static void do_decode(int nlhs, mxArray *plhs[],
                      int nrhs, const mxArray *prhs[]) {
    if (nrhs < 2)
        mexErrMsgIdAndTxt("ctoon:badArg", "decode: string argument required.");
    char *s = require_str(prhs[1], 2);
    size_t slen = strlen(s);
    ctoon_read_err err;
    memset(&err, 0, sizeof(err));
    ctoon_doc *doc = ctoon_read(s, slen, CTOON_READ_NOFLAG);
    mxFree(s);
    if (!doc)
        mexErrMsgIdAndTxt("ctoon:decodeError",
            "Failed to parse TOON string (code %u, pos %u).",
            err.code, (unsigned)err.pos);
    if (nlhs > 0) plhs[0] = val_to_mx(ctoon_doc_get_root(doc));
    ctoon_doc_free(doc);
}

static void do_read(int nlhs, mxArray *plhs[],
                    int nrhs, const mxArray *prhs[]) {
    if (nrhs < 2)
        mexErrMsgIdAndTxt("ctoon:badArg", "read: filepath argument required.");
    char *path = require_str(prhs[1], 2);
    ctoon_read_err err;
    memset(&err, 0, sizeof(err));
    ctoon_doc *doc = ctoon_read_file(path, CTOON_READ_NOFLAG, NULL, &err);
    mxFree(path);
    if (!doc)
        mexErrMsgIdAndTxt("ctoon:readError",
            "Failed to read TOON file (code %u).", err.code);
    if (nlhs > 0) plhs[0] = val_to_mx(ctoon_doc_get_root(doc));
    ctoon_doc_free(doc);
}

static void do_write(int nlhs, mxArray *plhs[],
                     int nrhs, const mxArray *prhs[]) {
    if (nrhs < 3)
        mexErrMsgIdAndTxt("ctoon:badArg", "write: value and filepath required.");
    ctoon_mut_doc *doc = doc_from_mx(prhs[1]);
    char *path = require_str(prhs[2], 3);
    ctoon_write_err werr;
    memset(&werr, 0, sizeof(werr));
    bool ok = ctoon_mut_write_file(path, doc, NULL, NULL, &werr);
    mxFree(path);
    ctoon_mut_doc_free(doc);
    if (!ok)
        mexErrMsgIdAndTxt("ctoon:writeError",
            "ctoon_mut_write_file() failed: %s",
            werr.msg ? werr.msg : "unknown error");
}

static void do_version(int nlhs, mxArray *plhs[],
                       int nrhs, const mxArray *prhs[]) {
    uint32_t v = ctoon_version();
    char buf[32];
    snprintf(buf, sizeof(buf), "%u.%u.%u",
             (v >> 16) & 0xFFu, (v >> 8) & 0xFFu, v & 0xFFu);
    if (nlhs > 0) plhs[0] = mxCreateString(buf);
}

/* =========================================================================
 * MEX entry point
 * ========================================================================= */

void mexFunction(int nlhs, mxArray *plhs[],
                 int nrhs, const mxArray *prhs[]) {
    if (nrhs < 1 || !mxIsChar(prhs[0]))
        mexErrMsgIdAndTxt("ctoon:badArg",
            "Internal gateway — use ctoon_encode / ctoon_decode / "
            "ctoon_read / ctoon_write instead.");

    char *cmd = mxArrayToString(prhs[0]);
    if (!cmd)
        mexErrMsgIdAndTxt("ctoon:memory", "mxArrayToString failed.");

    if      (strcmp(cmd, "encode")  == 0) do_encode(nlhs, plhs, nrhs, prhs);
    else if (strcmp(cmd, "decode")  == 0) do_decode(nlhs, plhs, nrhs, prhs);
    else if (strcmp(cmd, "read")    == 0) do_read(nlhs, plhs, nrhs, prhs);
    else if (strcmp(cmd, "write")   == 0) do_write(nlhs, plhs, nrhs, prhs);
    else if (strcmp(cmd, "version") == 0) do_version(nlhs, plhs, nrhs, prhs);
    else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Unknown internal command '%s'.", cmd);
        mxFree(cmd);
        mexErrMsgIdAndTxt("ctoon:badCmd", "%s", msg);
    }

    mxFree(cmd);
}
