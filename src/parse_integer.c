#include "config.h"

#include "quote.h"
#include "xstrtol.h"

#include "bgrep.h"

/*
 * Gratefully derived from "dd" (http://git.savannah.gnu.org/gitweb/?p=coreutils.git)
 *
 * The following function is licensed as follows:
 *    Copyright (C) 1985-2017 Free Software Foundation, Inc.
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    A copy of the GNU General Public License is available at
 *    <http://www.gnu.org/licenses/>.
 *
 * Return the value of STR, interpreted as a non-negative decimal integer,
 * optionally multiplied by various values.
 * Set *INVALID to a nonzero error value if STR does not represent a
 * number in this format.
 */
uintmax_t
parse_integer (const char *str, strtol_error *invalid)
{
        uintmax_t n;
        char *suffix;
        strtol_error e = xstrtoumax (str, &suffix, 10, &n, "bcEGkKMPTwYZ0");

        if (e == LONGINT_INVALID_SUFFIX_CHAR && *suffix == 'x')
        {
                uintmax_t multiplier = parse_integer (suffix + 1, invalid);

                if (multiplier != 0 && n * multiplier / multiplier != n)
                {
                        *invalid = LONGINT_OVERFLOW;
                        return 0;
                }

                if (n == 0 && STRPREFIX (str, "0x"))
                        error (0, 0,
                                "warning: %s is a zero multiplier; "
                                "use %s if that is intended",
                                quote_n (0, "0x"), quote_n (1, "00x"));

                n *= multiplier;
        }
        else if (e != LONGINT_OK)
        {
                *invalid = e;
                return 0;
        }

        return n;
}

