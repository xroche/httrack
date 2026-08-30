dnl @synopsis HTS_EXPAND_LIBDIR()
dnl
dnl Expand $exec_prefix and $libdir into hts_exec_prefix and hts_libdir. Until
dnl config.status runs, $prefix is NONE and $libdir still holds an unexpanded
dnl ${exec_prefix} reference, so neither can be read as a path.

AC_DEFUN([HTS_EXPAND_LIBDIR], [
hts_save_prefix=$prefix
hts_save_exec_prefix=$exec_prefix
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix=$prefix
eval hts_exec_prefix=\"$exec_prefix\"
eval hts_libdir=\"$libdir\"
eval hts_libdir=\"$hts_libdir\"
prefix=$hts_save_prefix
exec_prefix=$hts_save_exec_prefix
])
