/*
 * Copyright (c) 2007, Novell Inc.
 *
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information
 */

#define DO_ARRAY 1

#define _GNU_SOURCE
#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <expat.h>

#include "pool.h"
#include "repo.h"
#include "repo_updateinfoxml.h"

#define DISABLE_SPLIT
#include "tools_util.h"

/* #define DUMPOUT 1 */

/*
 * <deltainfo>
 *   <newpackage name="libtool" epoch="0" version="1.5.24" release="6.fc9" arch="i386">
 *     <delta oldepoch="0" oldversion="1.5.24" oldrelease="3.fc8">
 *       <filename>DRPMS/libtool-1.5.24-3.fc8_1.5.24-6.fc9.i386.drpm</filename>
 *       <sequence>libtool-1.5.24-3.fc8-d3571f98b048b1a870e40241bb46c67ab4</sequence>
 *       <size>22452</size>
 *       <checksum type="sha">8f05394695dee9399c204614e21e5f6848990ab7</checksum>
 *     </delta>
 *     <delta oldepoch="0" oldversion="1.5.22" oldrelease="11.fc7">
 *       <filename>DRPMS/libtool-1.5.22-11.fc7_1.5.24-6.fc9.i386.drpm</filename>
 *        <sequence>libtool-1.5.22-11.fc7-e82691677eee1e83b4812572c5c9ce8eb</sequence>
 *        <size>110362</size>
 *        <checksum type="sha">326658fee45c0baec1e70231046dbaf560f941ce</checksum>
 *      </delta>
 *    </newpackage>
 *  </deltainfo>
 */

enum state {
  STATE_START,
  STATE_NEWPACKAGE,     /* 1 */
  STATE_DELTA,          /* 2 */
  STATE_FILENAME,       /* 3 */
  STATE_SEQUENCE,       /* 4 */
  STATE_SIZE,           /* 5 */
  STATE_CHECKSUM,       /* 6 */
  STATE_LOCATION,       /* 7 */
  NUMSTATES
};

struct stateswitch {
  enum state from;
  char *ename;
  enum state to;
  int docontent;
};

/* !! must be sorted by first column !! */
static struct stateswitch stateswitches[] = {
  /* compatibility with old yum-presto */
  { STATE_START,       "prestodelta",     STATE_START, 0 },
  { STATE_START,       "deltainfo",       STATE_START, 0 },
  { STATE_START,       "newpackage",      STATE_NEWPACKAGE,  0 },
  { STATE_NEWPACKAGE,  "delta",           STATE_DELTA,       0 },
  /* compatibility with yum-presto */
  { STATE_DELTA,       "filename",        STATE_FILENAME,    1 },
  { STATE_DELTA,       "location",        STATE_LOCATION,    0 },
  { STATE_DELTA,       "sequence",        STATE_SEQUENCE,    1 },
  { STATE_DELTA,       "size",            STATE_SIZE,        1 },
  { STATE_DELTA,       "checksum",        STATE_CHECKSUM,    1 },
  { NUMSTATES }
};

/* Cumulated info about the current deltarpm or patchrpm */
struct deltarpm {
  Id locdir;
  Id locname;
  Id locevr;
  Id locsuffix;
  unsigned buildtime;
  unsigned downloadsize, archivesize;
  char *filechecksum;
  int filechecksumtype;
  /* Baseversion.  deltarpm only has one. */
  Id *bevr;
  unsigned nbevr;
  Id seqname;
  Id seqevr;
  char *seqnum;
};

struct parsedata {
  int depth;
  enum state state;
  int statedepth;
  char *content;
  int lcontent;
  int acontent;
  int docontent;
  Pool *pool;
  Repo *repo;
  Repodata *data;
  
  struct stateswitch *swtab[NUMSTATES];
  enum state sbtab[NUMSTATES];
  char *tempstr;
  int ltemp;
  int atemp;
  struct deltarpm delta;
  Id newpkgevr;
  Id newpkgname;
  Id newpkgarch;

  Id *handles;
  int nhandles;
};

/*
 * find attribute
 */

static const char *
find_attr(const char *txt, const char **atts)
{
  for (; *atts; atts += 2)
    {
      if (!strcmp(*atts, txt))
        return atts[1];
    }
  return 0;
}


/*
 * create evr (as Id) from 'epoch', 'version' and 'release' attributes
 */

static Id
makeevr_atts(Pool *pool, struct parsedata *pd, const char **atts)
{
  const char *e, *v, *r, *v2;
  char *c;
  int l;

  e = v = r = 0;
  for (; *atts; atts += 2)
    {
      if (!strcmp(*atts, "oldepoch"))
        e = atts[1];
      else if (!strcmp(*atts, "epoch"))
	e = atts[1];
      else if (!strcmp(*atts, "version"))
	v = atts[1];
      else if (!strcmp(*atts, "oldversion"))
	v = atts[1];
      else if (!strcmp(*atts, "release"))
	r = atts[1];
      else if (!strcmp(*atts, "oldrelease"))
	r = atts[1];
    }
  if (e && !strcmp(e, "0"))
    e = 0;
  if (v && !e)
    {
      for (v2 = v; *v2 >= '0' && *v2 <= '9'; v2++)
        ;
      if (v2 > v && *v2 == ':')
	e = "0";
    }
  l = 1;
  if (e)
    l += strlen(e) + 1;
  if (v)
    l += strlen(v);
  if (r)
    l += strlen(r) + 1;
  if (l > pd->acontent)
    {
      pd->content = sat_realloc(pd->content, l + 256);
      pd->acontent = l + 256;
    }
  c = pd->content;
  if (e)
    {
      strcpy(c, e);
      c += strlen(c);
      *c++ = ':';
    }
  if (v)
    {
      strcpy(c, v);
      c += strlen(c);
    }
  if (r)
    {
      *c++ = '-';
      strcpy(c, r);
      c += strlen(c);
    }
  *c = 0;
  if (!*pd->content)
    return 0;
#if 0
  fprintf(stderr, "evr: %s\n", pd->content);
#endif
  return str2id(pool, pd->content, 1);
}

static void parse_delta_location( struct parsedata *pd, 
                                  const char* str )
{
  Pool *pool = pd->pool;
  if (str)
    {
      /* Separate the filename into its different parts.
	 rpm/x86_64/alsa-1.0.14-31_31.2.x86_64.delta.rpm
	 --> dir = rpm/x86_64
	 name = alsa
	 evr = 1.0.14-31_31.2
	 suffix = x86_64.delta.rpm.  */
      char *real_str = strdup(str);
      char *s = real_str;
      char *s1, *s2;
      s1 = strrchr (s, '/');
      if (s1)
	{
	  pd->delta.locdir = strn2id(pool, s, s1 - s, 1);
	  s = s1 + 1;
	}
      /* Guess suffix.  */
      s1 = strrchr (s, '.');
      if (s1)
	{
	  for (s2 = s1 - 1; s2 > s; s2--)
	    if (*s2 == '.')
	      break;
	  if (!strcmp (s2, ".delta.rpm") || !strcmp (s2, ".patch.rpm"))
	    {
	      s1 = s2;
	      /* We accept one more item as suffix.  */
	      for (s2 = s1 - 1; s2 > s; s2--)
		if (*s2 == '.')
		  break;
	      s1 = s2;
	    }
	  if (*s1 == '.')
	    *s1++ = 0;
	  pd->delta.locsuffix = str2id(pool, s1, 1); 
	}
      /* Last '-'.  */
      s1 = strrchr (s, '-');
      if (s1)
	{
	  /* Second to last '-'.  */
	  for (s2 = s1 - 1; s2 > s; s2--)
	    if (*s2 == '-')
	      break;
	}
      else
	s2 = 0;
      if (s2 > s && *s2 == '-')
	{
	  *s2++ = 0;
	  pd->delta.locevr = str2id(pool, s2, 1);
	}
      pd->delta.locname = str2id(pool, s, 1);
      free(real_str);
    }
}
                                 
static void XMLCALL
startElement(void *userData, const char *name, const char **atts)
{
  struct parsedata *pd = userData;
  Pool *pool = pd->pool;
  struct stateswitch *sw;
  const char *str;

#if 0
  fprintf(stderr, "start: [%d]%s\n", pd->state, name);
#endif
  if (pd->depth != pd->statedepth)
    {
      pd->depth++;
      return;
    }

  pd->depth++;
  if (!pd->swtab[pd->state])
    return;
  for (sw = pd->swtab[pd->state]; sw->from == pd->state; sw++)  /* find name in statetable */
    if (!strcmp(sw->ename, name))
      break;
  if (sw->from != pd->state)
    {
#if 0
      fprintf(stderr, "into unknown: [%d]%s (from: %d)\n", sw->to, name, sw->from);
      exit( 1 );
#endif
      return;
    }
  pd->state = sw->to;
  pd->docontent = sw->docontent;
  pd->statedepth = pd->depth;
  pd->lcontent = 0;
  *pd->content = 0;

  switch(pd->state)
    {
    case STATE_START:
      break;
    case STATE_NEWPACKAGE:
      if ((str = find_attr("name", atts)) != 0)
	{
	  pd->newpkgname = str2id(pool, str, 1);
	}
      pd->newpkgevr = makeevr_atts(pool, pd, atts);
      if ((str = find_attr("arch", atts)) != 0)
	{
	  pd->newpkgarch = str2id(pool, str, 1);
	}
      break;

    case STATE_DELTA:
      memset(&pd->delta, 0, sizeof(pd->delta));
      *pd->tempstr = 0;
      pd->ltemp = 0;
      pd->delta.bevr = sat_extend(pd->delta.bevr, pd->delta.nbevr, 1, sizeof(Id), 7);
      pd->delta.bevr[pd->delta.nbevr++] = makeevr_atts(pool, pd, atts);
      break;
    case STATE_FILENAME:
      break;
    case STATE_LOCATION:
      parse_delta_location(pd, find_attr("href", atts));
      break;
    case STATE_SIZE:
      break;
    case STATE_CHECKSUM:
      pd->delta.filechecksum = 0;
      pd->delta.filechecksumtype = REPOKEY_TYPE_SHA1;
      if ((str = find_attr("type", atts)) != 0)
	{
	  if (!strcasecmp(str, "sha"))
	    pd->delta.filechecksumtype = REPOKEY_TYPE_SHA1;
	  else if (!strcasecmp(str, "sha256"))
	    pd->delta.filechecksumtype = REPOKEY_TYPE_SHA256;
	  else if (!strcasecmp(str, "md5"))
	    pd->delta.filechecksumtype = REPOKEY_TYPE_MD5;
	  else
	    pool_debug(pool, SAT_ERROR, "unknown checksum type: '%s'\n", str);
	}
    case STATE_SEQUENCE:
      break;
    default:
      break;
    }
}


static void XMLCALL
endElement(void *userData, const char *name)
{
  struct parsedata *pd = userData;
  Pool *pool = pd->pool;
  const char *str;

#if 0
  fprintf(stderr, "end: %s\n", name);
#endif
  if (pd->depth != pd->statedepth)
    {
      pd->depth--;
#if 0
      fprintf(stderr, "back from unknown %d %d %d\n", pd->state, pd->depth, pd->statedepth);
#endif
      return;
    }

  pd->depth--;
  pd->statedepth--;
  switch (pd->state)
    {
    case STATE_START:
      break;
    case STATE_NEWPACKAGE:
      break;
    case STATE_DELTA:
      {
	/* read all data for a deltarpm. commit into attributes */
	Id handle;
	struct deltarpm *d = &pd->delta;
#ifdef DUMPOUT
	int i;
#endif

#ifdef DUMPOUT

	fprintf (stderr, "found deltarpm for %s:\n", id2str(pool, pd->newpkgname));
#endif
	handle = repodata_new_handle(pd->data);
	/* we commit all handles later on in one go so that the
         * repodata code doesn't need to realloc every time */
	pd->handles = sat_extend(pd->handles, pd->nhandles, 1, sizeof(Id), 63);
        pd->handles[pd->nhandles++] = handle;
	repodata_set_id(pd->data, handle, DELTA_PACKAGE_NAME, pd->newpkgname);
	repodata_set_id(pd->data, handle, DELTA_PACKAGE_EVR, pd->newpkgevr);
	repodata_set_id(pd->data, handle, DELTA_PACKAGE_ARCH, pd->newpkgarch);
	repodata_set_id(pd->data, handle, DELTA_LOCATION_NAME, d->locname);
	repodata_set_id(pd->data, handle, DELTA_LOCATION_DIR, d->locdir);
	repodata_set_id(pd->data, handle, DELTA_LOCATION_EVR, d->locevr);
	repodata_set_id(pd->data, handle, DELTA_LOCATION_SUFFIX, d->locsuffix);
	if (d->downloadsize)
	  repodata_set_num(pd->data, handle, DELTA_DOWNLOADSIZE, (d->downloadsize + 1023) / 1024);
	if (d->filechecksum)
	  repodata_set_checksum(pd->data, handle, DELTA_CHECKSUM, d->filechecksumtype, d->filechecksum);
#ifdef DUMPOUT
	fprintf (stderr, "   loc: %s %s %s %s\n", id2str(pool, d->locdir),
		 id2str(pool, d->locname), id2str(pool, d->locevr),
		 id2str(pool, d->locsuffix));
	fprintf (stderr, "  size: %d down\n", d->downloadsize);
	fprintf (stderr, "  chek: %s\n", d->filechecksum);
#endif

	if (d->seqnum)
	  {
#ifdef DUMPOUT
	    fprintf (stderr, "  base: %s\n",
		     id2str(pool, d->bevr[0]));
	    fprintf (stderr, "            seq: %s\n",
		     id2str(pool, d->seqname));
	    fprintf (stderr, "                 %s\n",
		     id2str(pool, d->seqevr));
	    fprintf (stderr, "                 %s\n",
		     d->seqnum);
#endif
	    repodata_set_id(pd->data, handle, DELTA_BASE_EVR, d->bevr[0]);
	    repodata_set_id(pd->data, handle, DELTA_SEQ_NAME, d->seqname);
	    repodata_set_id(pd->data, handle, DELTA_SEQ_EVR, d->seqevr);
	    /* should store as binary blob! */
	    repodata_set_str(pd->data, handle, DELTA_SEQ_NUM, d->seqnum);

#ifdef DUMPOUT
	    fprintf(stderr, "OK\n");
#endif

#ifdef DUMPOUT              
	    if (d->seqevr != d->bevr[0])
	      fprintf (stderr, "XXXXX evr\n");
	    /* Name of package ("xxxx") should match the sequence info
	       name.  */
	    if (strcmp(id2str(pool, d->seqname), id2str(pool, pd->newpkgname)))
	      fprintf (stderr, "XXXXX name\n");
#endif
	  }
	else
	  {

#ifdef DUMPOUT                          
	    fprintf (stderr, "  base:");
	    for (i = 0; i < d->nbevr; i++)
	      fprintf (stderr, " %s", id2str(pool, d->bevr[i]));
	    fprintf (stderr, "\n");
#endif
	  }

      }
      pd->delta.filechecksum = sat_free(pd->delta.filechecksum);
      pd->delta.bevr = sat_free(pd->delta.bevr);
      pd->delta.nbevr = 0;
      pd->delta.seqnum = sat_free(pd->delta.seqnum);
      break;
    case STATE_FILENAME:
      parse_delta_location(pd, pd->content);
      break;
    case STATE_CHECKSUM:
      pd->delta.filechecksum = strdup(pd->content);
      break;
    case STATE_SIZE:
      pd->delta.downloadsize = atoi(pd->content);
      break;
    case STATE_SEQUENCE:
      if ((str = pd->content))
	{
	  const char *s1, *s2;
	  s1 = strrchr(str, '-');
	  if (s1)
	    {
	      for (s2 = s1 - 1; s2 > str; s2--)
	        if (*s2 == '-')
		  break;
	      if (*s2 == '-')
		{
		  for (s2 = s2 - 1; s2 > str; s2--)
		    if (*s2 == '-')
		      break;
		  if (*s2 == '-')
		    {
		      pd->delta.seqevr = strn2id(pool, s2 + 1, s1 - s2 - 1, 1);
		      pd->delta.seqname = strn2id(pool, str, s2 - str, 1);
		      str = s1 + 1;
		    }
		}
	    }
	  pd->delta.seqnum = strdup(str);
      }
    default:
      break;
    }

  pd->state = pd->sbtab[pd->state];
  pd->docontent = 0;
}


static void XMLCALL
characterData(void *userData, const XML_Char *s, int len)
{
  struct parsedata *pd = userData;
  int l;
  char *c;
  if (!pd->docontent)
    return;
  l = pd->lcontent + len + 1;
  if (l > pd->acontent)
    {
      pd->content = sat_realloc(pd->content, l + 256);
      pd->acontent = l + 256;
    }
  c = pd->content + pd->lcontent;
  pd->lcontent += len;
  while (len-- > 0)
    *c++ = *s++;
  *c = 0;
}

#define BUFF_SIZE 8192

void
repo_add_deltainfoxml(Repo *repo, FILE *fp, int flags)
{
  Pool *pool = repo->pool;
  struct parsedata pd;
  char buf[BUFF_SIZE];
  int i, l;
  struct stateswitch *sw;
  Repodata *data;

  data = repo_add_repodata(repo, flags);

  memset(&pd, 0, sizeof(pd));
  for (i = 0, sw = stateswitches; sw->from != NUMSTATES; i++, sw++)
    {
      if (!pd.swtab[sw->from])
        pd.swtab[sw->from] = sw;
      pd.sbtab[sw->to] = sw->from;
    }
  pd.pool = pool;
  pd.repo = repo;
  pd.data = data;

  pd.content = sat_malloc(256);
  pd.acontent = 256;
  pd.lcontent = 0;
  pd.tempstr = malloc(256);
  pd.atemp = 256;
  pd.ltemp = 0;

  XML_Parser parser = XML_ParserCreate(NULL);
  XML_SetUserData(parser, &pd);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, characterData);
  for (;;)
    {
      l = fread(buf, 1, sizeof(buf), fp);
      if (XML_Parse(parser, buf, l, l == 0) == XML_STATUS_ERROR)
	{
	  pool_debug(pool, SAT_FATAL, "repo_updateinfoxml: %s at line %u:%u\n", XML_ErrorString(XML_GetErrorCode(parser)), (unsigned int)XML_GetCurrentLineNumber(parser), (unsigned int)XML_GetCurrentColumnNumber(parser));
	  exit(1);
	}
      if (l == 0)
	break;
    }
  XML_ParserFree(parser);
  sat_free(pd.content);
  sat_free(pd.tempstr);
  join_freemem();

  /* now commit all handles */
  for (i = 0; i < pd.nhandles; i++)
    repodata_add_flexarray(pd.data, SOLVID_META, REPOSITORY_DELTAINFO, pd.handles[i]);
  sat_free(pd.handles);

  if (!(flags & REPO_NO_INTERNALIZE))
    repodata_internalize(data);
}

/* EOF */
