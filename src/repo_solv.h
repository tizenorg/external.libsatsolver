/*
 * Copyright (c) 2007, Novell Inc.
 *
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information
 */

/*
 * repo_solv.h
 *
 */

#ifndef SATSOLVER_REPO_SOLVE_H
#define SATSOLVER_REPO_SOLVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "pool.h"
#include "repo.h"

extern int repo_add_solv(Repo *repo, FILE *fp);
extern int repo_add_solv_flags(Repo *repo, FILE *fp, int flags);

#define SOLV_ADD_NO_STUBS	(1 << 8)

#ifdef __cplusplus
}
#endif

#endif /* SATSOLVER_REPO_SOLVE_H */
