#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OmawebBlocker OmawebBlocker;

// What the lists have to say about one request. The three answers are
// independent: a `redirect-rule` names a substitute that is served only when
// some other rule blocks, and a `removeparam` rule strips tracking parameters
// off a request that is going out either way.
typedef struct OmawebBlockerDecision {
    bool blocked;
    // The substitute to serve instead, by name, or null when the request is
    // simply refused. Never set without `blocked`.
    char *substitute;
    // The request address with tracking parameters removed, or null when no
    // rule changed it. Never set together with `blocked`.
    char *rewritten_url;
} OmawebBlockerDecision;

OmawebBlocker *omaweb_blocker_compile(const char *rules, char **report);
void omaweb_blocker_destroy(OmawebBlocker *blocker);
void omaweb_blocker_check(const OmawebBlocker *blocker, const char *url, const char *source_url,
    const char *resource_type, OmawebBlockerDecision *decision);
void omaweb_blocker_decision_release(OmawebBlockerDecision *decision);
char *omaweb_blocker_substitute(const char *name);
bool omaweb_blocker_matches_popup(
    const OmawebBlocker *blocker, const char *url, const char *opener_url);
char *omaweb_blocker_cosmetic_css(const OmawebBlocker *blocker, const char *url);
char *omaweb_blocker_scriptlet_source(const OmawebBlocker *blocker, const char *url);
bool omaweb_blocker_cosmetic_survey_wanted(const OmawebBlocker *blocker, const char *url);
char *omaweb_blocker_generic_cosmetic_css(
    const OmawebBlocker *blocker, const char *url, const char *classes, const char *ids);
void omaweb_blocker_string_free(char *value);

#ifdef __cplusplus
}
#endif
