/*
 * BRLTTY - A background process providing access to the Linux console (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2004 by The BRLTTY Team. All rights reserved.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#ifndef _tbl_load_H
#define _tbl_load_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*TranslationTableReporter) (const char *message);

#define TBL_UNDEFINED 0X1
#define TBL_DUPLICATE 0X2

extern int loadTranslationTable (
  const char *file,
  TranslationTable *table,
  TranslationTableReporter report,
  int options
);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _tbl_load_H */
