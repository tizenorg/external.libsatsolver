/*
 * Copyright (c) 2007, Novell Inc.
 *
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int with_attr = 0;

#include "pool.h"
#include "repo_solv.h"

static int dump_repoattrs_cb(void *vcbdata, Solvable *s, Repodata *data, Repokey *key, KeyValue *kv);

static void
dump_repodata(Repo *repo)
{
  unsigned i;
  Repodata *data;
  if (repo->nrepodata == 0)
    return;
  printf("repo contains %d repodata sections:\n", repo->nrepodata);
  for (i = 0, data = repo->repodata; i < repo->nrepodata; i++, data++)
    {
      unsigned int j;
      printf("\nrepodata %d has %d keys, %d schemata\n", i + 1, data->nkeys - 1, data->nschemata - 1);
      for (j = 1; j < data->nkeys; j++)
        printf("  %s (type %s size %d storage %d)\n", id2str(repo->pool, data->keys[j].name), id2str(repo->pool, data->keys[j].type), data->keys[j].size, data->keys[j].storage);
      if (data->localpool)
	printf("  localpool has %d strings, size is %d\n", data->spool.nstrings, data->spool.sstrings);
      if (data->dirpool.ndirs)
	printf("  localpool has %d directories\n", data->dirpool.ndirs);
      printf("\n");
      repodata_search(data, SOLVID_META, 0, SEARCH_ARRAYSENTINEL|SEARCH_SUB, dump_repoattrs_cb, 0);
    }
  printf("\n");
}

#if 0
static void
printids(Repo *repo, char *kind, Offset ido)
{
  Pool *pool = repo->pool;
  Id id, *ids;
  if (!ido)
    return;
  printf("%s:\n", kind);
  ids = repo->idarraydata + ido;
  while((id = *ids++) != 0)
    printf("  %s\n", dep2str(pool, id));
}
#endif

int
dump_attr(Repo *repo, Repodata *data, Repokey *key, KeyValue *kv)
{
  const char *keyname;
  KeyValue *kvp;
  int indent = 0;

  keyname = id2str(repo->pool, key->name);
  for (kvp = kv; (kvp = kvp->parent) != 0; indent += 2)
    printf("  ");
  switch(key->type)
    {
    case REPOKEY_TYPE_ID:
      if (data && data->localpool)
	kv->str = stringpool_id2str(&data->spool, kv->id);
      else
        kv->str = id2str(repo->pool, kv->id);
      printf("%s: %s\n", keyname, kv->str);
      break;
    case REPOKEY_TYPE_CONSTANTID:
      printf("%s: %s\n", keyname, dep2str(repo->pool, kv->id));
      break;
    case REPOKEY_TYPE_IDARRAY:
      if (!kv->entry)
        printf("%s:\n%*s", keyname, indent, "");
      if (data && data->localpool)
        printf("  %s\n", stringpool_id2str(&data->spool, kv->id));
      else
        printf("  %s\n", dep2str(repo->pool, kv->id));
      break;
    case REPOKEY_TYPE_STR:
      printf("%s: %s\n", keyname, kv->str);
      break;
    case REPOKEY_TYPE_MD5:
    case REPOKEY_TYPE_SHA1:
    case REPOKEY_TYPE_SHA256:
      printf("%s: %s (%s)\n", keyname, repodata_chk2str(data, key->type, (unsigned char *)kv->str), id2str(repo->pool, key->type));
      break;
    case REPOKEY_TYPE_VOID:
      printf("%s: (void)\n", keyname);
      break;
    case REPOKEY_TYPE_U32:
    case REPOKEY_TYPE_NUM:
    case REPOKEY_TYPE_CONSTANT:
      printf("%s: %d\n", keyname, kv->num);
      break;
    case REPOKEY_TYPE_BINARY:
      if (kv->num)
        printf("%s: %02x..%02x len %d\n", keyname, (unsigned char)kv->str[0], (unsigned char)kv->str[kv->num - 1], kv->num);
      else
        printf("%s: len 0\n", keyname);
      break;
    case REPOKEY_TYPE_DIRNUMNUMARRAY:
      if (!kv->entry)
        printf("%s:\n%*s", keyname, indent, "");
      printf("  %s %d %d\n", repodata_dir2str(data, kv->id, 0), kv->num, kv->num2);
      break;
    case REPOKEY_TYPE_DIRSTRARRAY:
      if (!kv->entry)
        printf("%s:\n%*s", keyname, indent, "");
      printf("  %s\n", repodata_dir2str(data, kv->id, kv->str));
      break;
    case REPOKEY_TYPE_FIXARRAY:
    case REPOKEY_TYPE_FLEXARRAY:
      if (!kv->entry)
        printf("%s:\n", keyname);
      else
        printf("\n");
      break;
    default:
      printf("%s: ?\n", keyname);
      break;
    }
  return 0;
}

#if 1
static int
dump_repoattrs_cb(void *vcbdata, Solvable *s, Repodata *data, Repokey *key, KeyValue *kv)
{
  if (key->name == REPOSITORY_SOLVABLES)
    return SEARCH_NEXT_SOLVABLE;
  return dump_attr(data->repo, data, key, kv);
}
#endif

/*
 * dump all attributes for Id <p>
 */

void
dump_repoattrs(Repo *repo, Id p)
{
#if 0
  repo_search(repo, p, 0, 0, SEARCH_ARRAYSENTINEL|SEARCH_SUB, dump_repoattrs_cb, 0);
#else
  Dataiterator di;
  dataiterator_init(&di, repo->pool, repo, p, 0, 0, SEARCH_ARRAYSENTINEL|SEARCH_SUB);
  while (dataiterator_step(&di))
    dump_attr(repo, di.data, di.key, &di.kv);
#endif
}

#if 0
void
dump_some_attrs(Repo *repo, Solvable *s)
{
  const char *summary = 0;
  unsigned int medianr = -1, downloadsize = -1;
  unsigned int time = -1;
  summary = repo_lookup_str(s, SOLVABLE_SUMMARY);
  medianr = repo_lookup_num(s, SOLVABLE_MEDIANR);
  downloadsize = repo_lookup_num (s, SOLVABLE_DOWNLOADSIZE);
  time = repo_lookup_num(s, SOLVABLE_BUILDTIME);
  printf ("  XXX %d %d %u %s\n", medianr, downloadsize, time, summary);
}
#endif


static int
loadcallback (Pool *pool, Repodata *data, void *vdata)
{
  FILE *fp = 0;
  int r;

printf("LOADCALLBACK\n");
  const char *location = repodata_lookup_str(data, SOLVID_META, REPOSITORY_LOCATION);
printf("loc %s\n", location);
  if (!location || !with_attr)
    return 0;
  fprintf (stderr, "Loading SOLV file %s\n", location);
  fp = fopen (location, "r");
  if (!fp)
    {
      perror(location);
      return 0;
    }
  r = repo_add_solv_flags(data->repo, fp, REPO_USE_LOADING|REPO_LOCALPOOL);
  fclose(fp);
  return !r ? 1 : 0;
}


static void
usage(int status)
{
  fprintf( stderr, "\nUsage:\n"
	   "dumpsolv [-a] [<solvfile>]\n"
	   "  -a  read attributes.\n"
	   );
  exit(status);
}

#if 0
static void
tryme (Repo *repo, Id p, Id keyname, const char *match, int flags)
{
  Dataiterator di;
  dataiterator_init(&di, repo, p, keyname, match, flags);
  while (dataiterator_step(&di))
    {
      switch (di.key->type)
	{
	  case REPOKEY_TYPE_ID:
	  case REPOKEY_TYPE_IDARRAY:
	      if (di.data && di.data->localpool)
		di.kv.str = stringpool_id2str(&di.data->spool, di.kv.id);
	      else
		di.kv.str = id2str(repo->pool, di.kv.id);
	      break;
	  case REPOKEY_TYPE_STR:
	  case REPOKEY_TYPE_DIRSTRARRAY:
	      break;
	  default:
	      di.kv.str = 0;
	}
      fprintf (stdout, "found: %d:%s %d %s %d %d %d\n",
	       di.solvid,
	       id2str(repo->pool, di.key->name),
	       di.kv.id,
	       di.kv.str, di.kv.num, di.kv.num2, di.kv.eof);
    }
}
#endif

int main(int argc, char **argv)
{
  Repo *repo;
  Pool *pool;
  int c, i, j, n;
  Solvable *s;
  
  pool = pool_create();
  pool_setdebuglevel(pool, 1);
  pool_setloadcallback(pool, loadcallback, 0);

  while ((c = getopt(argc, argv, "ha")) >= 0)
    {
      switch(c)
	{
	case 'h':
	  usage(0);
	  break;
	case 'a':
	  with_attr = 1;
	  break;
	default:
          usage(1);
          break;
	}
    }
  for (; optind < argc; optind++)
    {
      if (freopen(argv[optind], "r", stdin) == 0)
	{
	  perror(argv[optind]);
	  exit(1);
	}
      repo = repo_create(pool, argv[optind]);
      if (repo_add_solv(repo, stdin))
	printf("could not read repository\n");
    }
  if (!pool->nrepos)
    {
      repo = repo_create(pool, argc != 1 ? argv[1] : "<stdin>");
      if (repo_add_solv(repo, stdin))
	printf("could not read repository\n");
    }
  printf("pool contains %d strings, %d rels, string size is %d\n", pool->ss.nstrings, pool->nrels, pool->ss.sstrings);

#if 0
{
  Dataiterator di;
  dataiterator_init(&di, repo, -1, 0, "oo", DI_SEARCHSUB|SEARCH_SUBSTRING);
  while (dataiterator_step(&di))
    dump_attr(di.repo, di.data, di.key, &di.kv);
  exit(0);
}
#endif

  n = 0;
  FOR_REPOS(j, repo)
    {
      dump_repodata(repo);

      printf("repo %d contains %d solvables\n", j, repo->nsolvables);
      printf("repo start: %d end: %d\n", repo->start, repo->end);
      FOR_REPO_SOLVABLES(repo, i, s)
	{
	  n++;
	  printf("\n");
	  printf("solvable %d (%d):\n", n, i);
#if 0
	  if (s->name || s->evr || s->arch)
	    printf("name: %s %s %s\n", id2str(pool, s->name), id2str(pool, s->evr), id2str(pool, s->arch));
	  if (s->vendor)
	    printf("vendor: %s\n", id2str(pool, s->vendor));
	  printids(repo, "provides", s->provides);
	  printids(repo, "obsoletes", s->obsoletes);
	  printids(repo, "conflicts", s->conflicts);
	  printids(repo, "requires", s->requires);
	  printids(repo, "recommends", s->recommends);
	  printids(repo, "suggests", s->suggests);
	  printids(repo, "supplements", s->supplements);
	  printids(repo, "enhances", s->enhances);
	  if (repo->rpmdbid)
	    printf("rpmdbid: %u\n", repo->rpmdbid[i - repo->start]);
#endif
	  dump_repoattrs(repo, i);
#if 0
	  dump_some_attrs(repo, s);
#endif
	}
#if 0
      tryme(repo, 0, SOLVABLE_MEDIANR, 0, 0);
      printf("\n");
      tryme(repo, 0, 0, 0, 0);
      printf("\n");
      tryme(repo, 0, 0, "*y*e*", SEARCH_GLOB);
#endif
    }
#if 0
  printf ("\nSearchresults:\n");
  Dataiterator di;
  dataiterator_init(&di, pool, 0, 0, 0, "3", SEARCH_SUB | SEARCH_SUBSTRING | SEARCH_FILES);
  //int count = 0;
  while (dataiterator_step(&di))
    {
      printf("%d:", di.solvid);
      dump_attr(repo, di.data, di.key, &di.kv);
      /*if (di.solvid == 4 && count++ == 0)
	dataiterator_jump_to_solvable(&di, pool->solvables + 3);*/
      //dataiterator_skip_attribute(&di);
      //dataiterator_skip_solvable(&di);
      //dataiterator_skip_repo(&di);
    }
#endif
  pool_free(pool);
  exit(0);
}
