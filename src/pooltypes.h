/*
 * Copyright (c) 2007, Novell Inc.
 *
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information
 */

/*
 * pooltypes.h
 * 
 */

#ifndef SATSOLVER_POOLTYPES_H
#define SATSOLVER_POOLTYPES_H

/* format version number for .solv files */
#define SOLV_VERSION_0 0
#define SOLV_VERSION_1 1
#define SOLV_VERSION_2 2
#define SOLV_VERSION_3 3
#define SOLV_VERSION_4 4
#define SOLV_VERSION_5 5
#define SOLV_VERSION_6 6
#define SOLV_VERSION_7 7
#define SOLV_VERSION_8 8

/* The format of .solv files might change incompatibly, and that is described
   by the above version number.  But sometimes we also extend the emitted
   attributes (e.g. by adding a new one for solvables, for instance patch
   category).  Consumers need to know if the .solv file they have needs to
   be regenerated by newer converters or not (or better, if regenerating them
   would give a different .solv file).  We use this serial number for that.
   We increase it every time we add or remove attributes (or change the
   interpretation of them).  Tools installed by the user will have their
   version compiled in, so they can detect mismatches between .solv files
   they see and themself.  */
#define SOLV_CONTENT_VERSION 1

#define SOLV_FLAG_PREFIX_POOL 4

struct _Stringpool;
typedef struct _Stringpool Stringpool;

struct _Pool;
typedef struct _Pool Pool;

// identifier for string values
typedef int Id;		/* must be signed!, since negative Id is used in solver rules to denote negation */

// offset value, e.g. used to 'point' into the stringspace
typedef unsigned int Offset;

#endif /* SATSOLVER_POOLTYPES_H */