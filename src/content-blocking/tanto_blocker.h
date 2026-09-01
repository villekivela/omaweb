#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TantoBlocker TantoBlocker;

// What the lists have to say about one request. The three answers are
// independent: a `redirect-rule` names a substitute that is served only when
// some other rule blocks, and a `removeparam` rule strips tracking parameters
// off a request that is going out either way.
typedef struct TantoBlockerDecision {
    bool blocked;
    // The substitute to serve instead, by name, or null when the request is
    // simply refused. Never set without `blocked`.
    char *substitute;
    // The request address with tracking parameters removed, or null when no
    // rule changed it. Never set together with `blocked`.
    char *rewritten_url;
} TantoBlockerDecision;

TantoBlocker *tanto_blocker_compile(const char *rules, char **report);
void tanto_blocker_destroy(TantoBlocker *blocker);
void tanto_blocker_check(const TantoBlocker *blocker, const char *url, const char *source_url,
    const char *resource_type, TantoBlockerDecision *decision);
void tanto_blocker_decision_release(TantoBlockerDecision *decision);
char *tanto_blocker_substitute(const char *name);
bool tanto_blocker_matches_popup(const TantoBlocker *blocker, const char *url,
    const char *opener_url);
char *tanto_blocker_cosmetic_css(const TantoBlocker *blocker, const char *url);
char *tanto_blocker_scriptlet_source(const TantoBlocker *blocker, const char *url);
bool tanto_blocker_cosmetic_survey_wanted(const TantoBlocker *blocker, const char *url);
char *tanto_blocker_generic_cosmetic_css(const TantoBlocker *blocker, const char *url,
    const char *classes, const char *ids);
void tanto_blocker_string_free(char *value);

#ifdef __cplusplus
}
#endif
