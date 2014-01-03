#include "vadr.h"

int _dots_length(SEXP dots);

SEXP _dots_unpack(SEXP dots) {
  int i;
  SEXP s;
  int length = 0;
  SEXP names, environments, expressions, values;
  //SEXP evaluated, codeptr, missing, wraplist;
  //SEXP seen;

  SEXP dataFrame;
  SEXP colNames;

  //check inputs and measure length
  length = _dots_length(dots);

  // unpack information for each item:
  // names, environemnts, expressions, values, evaluated, seen
  PROTECT(names = allocVector(STRSXP, length));
  PROTECT(environments = allocVector(VECSXP, length));
  PROTECT(expressions = allocVector(VECSXP, length));
  PROTECT(values = allocVector(VECSXP, length));

  for (s = dots, i = 0; i < length; s = CDR(s), i++) {
    if (TYPEOF(s) != DOTSXP && TYPEOF(s) != LISTSXP)
      error("Expected DOTSXP, got %s at index %d", type2char(TYPEOF(s)), i);

    SEXP item = CAR(s);

    if (TYPEOF(item) != PROMSXP)
      error("Expected PROMSXP as CAR of DOTSXP, got %s", type2char(TYPEOF(item)));

    // if we have an unevluated promise whose code is another promise, descend
    while ((PRENV(item) != R_NilValue) && (TYPEOF(PRCODE(item)) == PROMSXP)) {
      item = PRCODE(item);
    }

    if ((TYPEOF(PRENV(item)) != ENVSXP) && (PRENV(item) != R_NilValue))
      error("Expected ENVSXP or NULL in environment slot of DOTSXP, got %s",
            type2char(TYPEOF(item)));

    SET_STRING_ELT(names, i, isNull(TAG(s)) ? mkChar("") : asChar(TAG(s)));
    SET_VECTOR_ELT(environments, i, PRENV(item));
    SET_VECTOR_ELT(expressions, i, PREXPR(item));

    if (PRVALUE(item) != R_UnboundValue) {
      SET_VECTOR_ELT(values, i, PRVALUE(item));
    } else {
      SET_VECTOR_ELT(values, i, R_NilValue);
    }
  }

  PROTECT(dataFrame = allocVector(VECSXP, 4));
  SET_VECTOR_ELT(dataFrame, 0, names);
  SET_VECTOR_ELT(dataFrame, 1, environments);
  SET_VECTOR_ELT(dataFrame, 2, expressions);
  SET_VECTOR_ELT(dataFrame, 3, values);

  PROTECT(colNames = allocVector(STRSXP, 4));
  SET_STRING_ELT(colNames, 0, mkChar("name"));
  SET_STRING_ELT(colNames, 1, mkChar("envir"));
  SET_STRING_ELT(colNames, 2, mkChar("expr"));
  SET_STRING_ELT(colNames, 3, mkChar("value"));

  setAttrib(expressions, R_ClassSymbol, ScalarString(mkChar("deparse")));
  setAttrib(environments, R_ClassSymbol, ScalarString(mkChar("deparse")));
  setAttrib(values, R_ClassSymbol, ScalarString(mkChar("deparse")));

  setAttrib(dataFrame, R_NamesSymbol, colNames);
  setAttrib(dataFrame, R_RowNamesSymbol, names);
  setAttrib(dataFrame, R_ClassSymbol, ScalarString(mkChar("data.frame")));

  UNPROTECT(6);
  return(dataFrame);
}

SEXP _dots_names(SEXP dots) {
  SEXP names, s;
  int i, length;

  length = _dots_length(dots);

  int made = 0;
  names = R_NilValue;
  PROTECT(names = allocVector(STRSXP, length));

  for (s = dots, i = 0; i < length; s = CDR(s), i++) {
    if (isNull(TAG(s))) {
      SET_STRING_ELT(names, i, mkChar(""));
    } else {
      made = 1;
      SET_STRING_ELT(names, i, asChar(TAG(s)));
    }
  }
  UNPROTECT(1);
  
  return(made ? names : R_NilValue);
}

SEXP _as_dots_literal(SEXP list, SEXP dotlist) {
  if (length(dotlist) == 0) {
    dotlist = PROTECT(allocVector(VECSXP, 0));
    setAttrib(dotlist, R_ClassSymbol, ScalarString(mkChar("...")));
    UNPROTECT(1);
    return dotlist;
  }
  if (TYPEOF(list) != VECSXP)
    error("Expected list, got %s", type2char(TYPEOF(list)));
  if (TYPEOF(dotlist) != DOTSXP)
    error("Expected ..., got %s", type2char(TYPEOF(dotlist)));
  int len = length(list);
  SEXP names = getAttrib(list, R_NamesSymbol);
  int i;
  SEXP iter;
  /* destructively jam the values into the dotlist */
  for (i = 0, iter = dotlist;
       iter != R_NilValue && i < len;
       i++, iter = CDR(iter)) {
    if (TYPEOF(CAR(iter)) != PROMSXP)
      error("Expected promise, got %s", type2char(TYPEOF(CAR(iter))));
    SET_PRVALUE(CAR(iter), VECTOR_ELT(list, i));
    SET_PRCODE(CAR(iter), VECTOR_ELT(list, i));
    SET_PRENV(CAR(iter), R_NilValue);
    if ((names != R_NilValue) && (STRING_ELT(names, i) != R_BlankString)) {
      SET_TAG(iter, install(CHAR(STRING_ELT(names, i)) ));
    }
  }
  setAttrib(dotlist, R_ClassSymbol, ScalarString(mkChar("...")));
  return dotlist;
}

/* Convert a DOTSXP into a list of raw promise objects. */
SEXP _dotslist_to_list(SEXP x) {
  int i;
  SEXP output, names;
  int len = length(x);

  PROTECT(output = allocVector(VECSXP, len));
  PROTECT(names = allocVector(STRSXP, len));
  if (len > 0) {
    if (TYPEOF(x) != DOTSXP)
      error("Expected a ..., got %s", type2char(TYPEOF(x)));
  }
  for (i = 0; i < len; x=CDR(x), i++) {
    SET_VECTOR_ELT(output, i, CAR(x));
    SET_STRING_ELT(names, i, isNull(TAG(x)) ? R_BlankString : asChar(TAG(x)));
  }
  if (len > 0) {
    setAttrib(output, R_NamesSymbol, names);
  }
  
  UNPROTECT(2);
  return output;
}

/* Convert a list of promise objects into a DOTSXP. */
SEXP _list_to_dotslist(SEXP list) {
  assert_type(list, VECSXP);
  int len = length(list);
  int i;
  SEXP output, names;
  names = getAttrib(list, R_NamesSymbol);
  if (len > 0) {
    output = PROTECT(allocList(len));
    SEXP output_iter = output;
    for (i = 0; i < len; i++, output_iter=CDR(output_iter)) {
      SET_TYPEOF(output_iter, DOTSXP);
      if ((names != R_NilValue) && (STRING_ELT(names, i) != R_BlankString)) {
        SET_TAG(output_iter, install(CHAR(STRING_ELT(names, i)) ));
      }
      SETCAR(output_iter, VECTOR_ELT(list, i));
    }
  } else {
    output = PROTECT(allocVector(VECSXP, 0));
  }
  setAttrib(output, R_ClassSymbol, ScalarString(mkChar("...")));
  UNPROTECT(1);
  return output;
}

SEXP _mutate_expressions(SEXP dots, SEXP new_exprs) {
  assert_type(dots, DOTSXP);
  assert_type(new_exprs, VECSXP);

  int n = length(dots);
  if (LENGTH(new_exprs) != length(dots)) {
    error("Length mismatch");
  }
    
  /* Hackish. These get turned into dots and promises respectively... */
  SEXP out = PROTECT(allocList(length(dots)));
  SEXP promises = PROTECT(allocList(length(dots)));

  SEXP next_prom, p, o; int i;
  next_prom = promises;
  for(i = 0, p = dots, o = out;
      i < n;
      i++, p = CDR(p), o = CDR(o)) {
    SEXP newprom = next_prom;
    SEXP oldprom = CAR(p);
    while (TYPEOF(PRCODE(oldprom)) == PROMSXP) {
      oldprom = PRCODE(oldprom);
    }
    if (PRENV(oldprom) == R_NilValue) {
      error("Can't put new expression on already-evaluated promise");
    }
    next_prom = CDR(next_prom);
    
    SET_TYPEOF(o, DOTSXP);
    SET_TAG(o, TAG(p));
    SET_TYPEOF(newprom, PROMSXP);
    SET_PRCODE(newprom, VECTOR_ELT(new_exprs, i));
    SET_PRVALUE(newprom, PRVALUE(oldprom));
    SET_PRENV(newprom, PRENV(oldprom));
    SETCAR(o, newprom);
  }
  DUPLICATE_ATTRIB(out, dots);
  UNPROTECT(2);
  return out;
}

SEXP _mutate_environments(SEXP dots, SEXP new_envs) {
  assert_type(dots, DOTSXP);
  assert_type(new_envs, VECSXP);

  int n = length(dots);
  if (LENGTH(new_envs) != length(dots)) {
    error("Length mismatch");
  }
    
  /* Hackish. These get turned into dots and promises respectively... */
  SEXP out = PROTECT(allocList(LENGTH(new_envs)));
  SEXP promises = PROTECT(allocList(LENGTH(new_envs)));

  SEXP next_prom, p, o; int i;
  next_prom = promises;
  for(i = 0, p = dots, o = out;
      i < n;
      i++, p = CDR(p), o = CDR(o)) {
    SEXP newprom = next_prom;
    SEXP oldprom = CAR(p);
    next_prom = CDR(next_prom);

    while (TYPEOF(PRCODE(oldprom)) == PROMSXP) {
      oldprom = PRCODE(oldprom);
    }
    
    SET_TYPEOF(o, DOTSXP);
    SET_TAG(o, TAG(p));
    SET_TYPEOF(newprom, PROMSXP);
    assert_type(VECTOR_ELT(new_envs, i), ENVSXP);
    SET_PRENV(newprom, VECTOR_ELT(new_envs, i));
    SET_PRVALUE(newprom, R_UnboundValue);
    SET_PRCODE(newprom, PRCODE(oldprom));
    SET_PRSEEN(newprom, 0);
    SETCAR(o, newprom);
  }
  DUPLICATE_ATTRIB(out, dots);
  UNPROTECT(2);
  return out;
}

/*
 * Local Variables:
 * eval: (previewing-mode)
 * previewing-build-command: (previewing-run-R-unit-tests)
 * End:
 */
