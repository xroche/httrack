# =====================================================================================
#  https://www.gnu.org/software/autoconf-archive/ax_check_aligned_access_required.html
# =====================================================================================
#
# SYNOPSIS
#
#   AX_CHECK_ALIGNED_ACCESS_REQUIRED
#
# DESCRIPTION
#
#   While the x86 CPUs allow access to memory objects to be unaligned it
#   happens that most of the modern designs require objects to be aligned -
#   or they will fail with a buserror. That mode is quite known by
#   big-endian machines (sparc, etc) however the alpha cpu is little-
#   endian.
#
#   The following function will test for aligned access to be required and
#   set a config.h define HAVE_ALIGNED_ACCESS_REQUIRED (name derived by
#   standard usage). Structures loaded from a file (or mmapped to memory)
#   should be accessed per-byte in that case to avoid segfault type errors.
#
#   The function checks if unaligned access would ignore the lowest bit of
#   the address. If that happens or if the test binary crashes, aligned
#   access is required.
#
#   If cross-compiling, assume that aligned access is needed to be safe. Set
#   ax_cv_have_aligned_access_required=no to override that assumption.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

#serial 11

AC_DEFUN([AX_CHECK_ALIGNED_ACCESS_REQUIRED],
[AC_CACHE_CHECK([if pointers to integers require aligned access],
  [ax_cv_have_aligned_access_required],
  [AC_RUN_IFELSE([
    AC_LANG_PROGRAM([[@%:@include <stdlib.h>]],
                    [[
                      int i;
                      int *p;
                      int *q;
                      char *str;
                      str = (char *) malloc(40);
                      for (i = 0; i < 40; i++) {
                        *(str + i) = i;
                      }
                      p = (int *) (str + 1);
                      q = (int *) (str + 2);
                      return (*p == *q);
                    ]])],
     [ax_cv_have_aligned_access_required=no],
     [ax_cv_have_aligned_access_required=yes],
     [ax_cv_have_aligned_access_required=maybe])])

if test "x$ax_cv_have_aligned_access_required" = "xmaybe"; then
  AC_MSG_WARN([Assuming aligned access is required when cross-compiling])
  ax_cv_have_aligned_access_required=yes
fi

if test "x$ax_cv_have_aligned_access_required" = "xyes"; then
  AC_DEFINE([HAVE_ALIGNED_ACCESS_REQUIRED], [1],
    [Define if pointers to integers require aligned access])
fi
])
