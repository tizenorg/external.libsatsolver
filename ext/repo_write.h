/*
 * Copyright (c) 2007, Novell Inc.
 *
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information
 */

/*
 * repo_write.h
 *
 */

#ifndef REPO_WRITE_H
#define REPO_WRITE_H

#include <stdio.h>

#include "repo.h"

void repo_write(Repo *repo, FILE *fp, int (*keyfilter)(Repo *repo, Repokey *key, void *kfdata), void *kfdata, Id **keyarrayp);
int repo_write_stdkeyfilter(Repo *repo, Repokey *key, void *kfdata);

void repodata_write(Repodata *data , FILE *fp, int (*keyfilter)(Repo *repo, Repokey *key, void *kfdata), void *kfdata);

#endif
