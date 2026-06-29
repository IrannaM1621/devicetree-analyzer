#ifndef DTDEP_ERR_H
#define DTDEP_ERR_H

typedef enum {
    DTDEP_OK          =  0,
    DTDEP_ERR_IO      = -1,
    DTDEP_ERR_OOM     = -2,
    DTDEP_ERR_PARSE   = -3,
    DTDEP_ERR_RESOLVE = -4,
    DTDEP_ERR_CYCLE   = -5,
} dtdep_err_t;

static inline const char *dtdep_strerror(dtdep_err_t e) {
    switch (e) {
    case DTDEP_OK:          return "ok";
    case DTDEP_ERR_IO:      return "I/O error";
    case DTDEP_ERR_OOM:     return "out of memory";
    case DTDEP_ERR_PARSE:   return "parse error";
    case DTDEP_ERR_RESOLVE: return "resolve error";
    case DTDEP_ERR_CYCLE:   return "dependency cycle";
    default:                return "unknown error";
    }
}

#endif /* DTDEP_ERR_H */
