#ifndef _FILTER_H_
#define _FILTER_H_

#include "es.h"

struct filter_t
{
    /* Input format */
    es_format_t         fmt_in;

    /* Output format of filter */
    es_format_t         fmt_out;
};
#endif
