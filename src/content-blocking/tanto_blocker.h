#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TantoBlocker TantoBlocker;

TantoBlocker *tanto_blocker_compile(const char *rules, char **report);
void tanto_blocker_destroy(TantoBlocker *blocker);
bool tanto_blocker_matches(const TantoBlocker *blocker, const char *url,
    const char *source_url, const char *resource_type);
char *tanto_blocker_cosmetic_css(const TantoBlocker *blocker, const char *url);
void tanto_blocker_string_free(char *value);

#ifdef __cplusplus
}
#endif
