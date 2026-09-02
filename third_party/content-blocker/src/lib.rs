use adblock::{
    Engine,
    lists::{FilterSet, ParseOptions},
    request::Request,
    resources::{
        InMemoryResourceStorage, Resource, ResourceImpl, ResourceStorage, ResourceStorageBackend,
    },
};
use serde_json::json;
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::{CStr, CString, c_char};
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::sync::LazyLock;

// uBlock Origin's resource library, vendored and pinned under
// third_party/ubo-scriptlets and built into the binary. Two sets, and a rule
// names one of them: a `##+js(...)` rule contributes a scriptlet's name and
// the arguments to call it with, and a `$redirect=` rule contributes the name
// of a substitute body to serve in place of the request. No list ever supplies
// either the code that runs or the body that is served.
//
// Decoded once for the process rather than per rule set: the library is a
// constant, while a rule set is recompiled whenever a subscription updates.
static LIBRARY: LazyLock<Library> = LazyLock::new(|| {
    let mut resources: Vec<Resource> =
        serde_json::from_str(include_str!("../../ubo-scriptlets/scriptlets.json"))
            .expect("the vendored scriptlet library parses as resource descriptors");
    resources.extend(
        serde_json::from_str::<Vec<Resource>>(include_str!(
            "../../ubo-scriptlets/redirects.json"
        ))
        .expect("the vendored redirect resources parse as resource descriptors"),
    );
    Library::new(resources)
});

struct Library {
    storage: InMemoryResourceStorage,
    // Which names a rule may ask for. The engine would answer this too, but
    // only once a page asks; a list is reported on when it compiles, so the
    // names are kept apart from the code.
    injectable: BTreeSet<String>,
    // The scriptlets uBO only lets a list the user vouched for inject —
    // `trusted-set-cookie` sets any cookie to any value. Omaweb vouches for no
    // list, so every rule set compiles without permissions and the engine
    // refuses these. Kept apart from the unknown names so a report can say
    // which of the two happened.
    trust_gated: BTreeSet<String>,
    // Which names a `$redirect` rule may ask for, and the body each one serves
    // as a `data:` URL. Answered when a list compiles rather than when a page
    // asks, for the same reason as the injectable names.
    substitutes: BTreeMap<String, String>,
    // The same bodies read the other way. A match reports the body it chose
    // and never the name it came from, and the name is what Omaweb serves the
    // substitute under, so that a replaced request stays legible in a network
    // log as the resource that replaced it. Two resources with byte-identical
    // bodies and the same MIME type would share an entry and one of the two
    // names; upstream carries no such pair, and the bytes served would be
    // right either way.
    substitute_names: BTreeMap<String, String>,
}

impl Library {
    fn new(resources: Vec<Resource>) -> Self {
        let mut injectable = BTreeSet::new();
        let mut trust_gated = BTreeSet::new();
        let mut redirectable = Vec::new();
        for resource in &resources {
            if resource.kind.supports_redirect() && resource.permission.to_bits() == 0 {
                redirectable.push((resource.name.clone(), resource.aliases.clone()));
            }
            if !resource.kind.supports_scriptlet_injection() {
                continue;
            }
            let destination = if resource.permission.to_bits() == 0 {
                &mut injectable
            } else {
                &mut trust_gated
            };
            for name in std::iter::once(&resource.name).chain(resource.aliases.iter()) {
                destination.insert(name.clone());
            }
        }
        // The `data:` URL a match reports is the engine's own encoding of a
        // body, so both directions are read back out of the storage rather
        // than spelled a second way here.
        let storage = InMemoryResourceStorage::from_resources(resources);
        let bodies = ResourceStorage::from_backend(storage.clone());
        let mut substitutes = BTreeMap::new();
        let mut substitute_names = BTreeMap::new();
        for (name, aliases) in redirectable {
            let Some(body) = bodies.get_redirect_resource(&name) else {
                continue;
            };
            substitute_names.insert(body.clone(), name.clone());
            for alias in aliases {
                substitutes.insert(alias, body.clone());
            }
            substitutes.insert(name, body);
        }
        Self {
            storage,
            injectable,
            trust_gated,
            substitutes,
            substitute_names,
        }
    }

    // A rule may leave the `.js` off the name, the way the lists usually do.
    fn holds(names: &BTreeSet<String>, name: &str) -> bool {
        names.contains(name) || names.contains(&format!("{name}.js"))
    }

    // The body served in place of a request a `$redirect` rule refused, or
    // None for a name this build carries no resource for.
    fn substitute(&self, name: &str) -> Option<&str> {
        self.substitutes.get(name).map(String::as_str)
    }

    // The name the library serves this body under. Every body in the map came
    // out of the library, so a match that reports one always has a name.
    fn substitute_name(&self, body: &str) -> Option<&str> {
        self.substitute_names.get(body).map(String::as_str)
    }

    // Why this name will not run, or None when it will.
    fn refusal(&self, name: &str) -> Option<&'static str> {
        if Self::holds(&self.injectable, name) {
            None
        } else if Self::holds(&self.trust_gated, name) {
            Some("scriptlets requiring trust")
        } else {
            Some("scriptlets this build does not carry")
        }
    }
}

// Lends the one decoded library to an engine. adblock-rust owns its resource
// storage, and the alternative is handing each engine its own copy.
struct VendoredResources;

impl ResourceStorageBackend for VendoredResources {
    fn get_resource(&self, name: &str) -> Option<ResourceImpl> {
        LIBRARY.storage.get_resource(name)
    }
}

pub struct OmawebBlocker {
    engine: Engine,
    // A second engine holds the list's $popup rules with that option stripped
    // off. adblock-rust has no popup request type and rejects the option
    // outright, so the rules are kept apart and asked about separately, at the
    // moment a page asks for a window rather than during a page's requests.
    popups: Engine,
}

fn input(value: *const c_char) -> Option<String> {
    if value.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(value) }
        .to_str()
        .ok()
        .map(str::to_owned)
}

fn output(value: String) -> *mut c_char {
    CString::new(value).map_or(std::ptr::null_mut(), CString::into_raw)
}

// A cosmetic rule, hiding or scriptlet, and the exceptions that take either
// back. The separators are what identify one; there is no other marker.
fn is_cosmetic_rule(line: &str) -> bool {
    line.contains("##") || line.contains("#@#")
}

// The scriptlet a `+js(...)` rule asks for, or None for a line that asks for
// none. Everything after the name is that scriptlet's arguments, which may
// contain anything at all — including the markers a procedural selector uses.
fn scriptlet_name(line: &str) -> Option<&str> {
    if !line.contains("##+js(") && !line.contains("#@#+js(") {
        return None;
    }
    let (_, arguments) = line.split_once("+js(")?;
    let name = arguments.split([',', ')']).next()?.trim();
    (!name.is_empty()).then_some(name)
}

fn unsupported_category(line: &str) -> Option<&'static str> {
    let trimmed = line.trim();
    // Asked first, and answered by the library rather than by the text: a
    // scriptlet's arguments are opaque, so any marker further down this
    // function could appear inside them and mean nothing.
    if let Some(name) = scriptlet_name(trimmed) {
        return LIBRARY.refusal(name);
    }
    const PROCEDURAL_MARKERS: [&str; 10] = [
        "#?#",
        "#$#",
        ":has-text(",
        ":matches-attr(",
        ":matches-css(",
        ":min-text-length(",
        ":remove(",
        ":style(",
        ":upward(",
        ":xpath(",
    ];
    if PROCEDURAL_MARKERS
        .iter()
        .any(|marker| trimmed.contains(marker))
    {
        Some("procedural selectors")
    } else if trimmed.contains("##^") || trimmed.contains("$html") {
        Some("HTML filtering")
    } else if trimmed.contains("$replace") {
        Some("response rewriting")
    } else if trimmed.contains("$csp") {
        // A `$csp` rule adds a Content-Security-Policy header to a response.
        // A request interceptor never sees a response, so this is refused
        // rather than counted among what a list contributed.
        Some("content security policies")
    } else if substituted_name(trimmed)
        .is_some_and(|name| LIBRARY.substitute(name).is_none())
    {
        Some("substitutes this build does not carry")
    } else if !trimmed.starts_with('!')
        && !trimmed.starts_with('[')
        && trimmed.split_ascii_whitespace().count() >= 4
        && !trimmed.contains("||")
        && !is_cosmetic_rule(trimmed)
    {
        Some("dynamic rules")
    } else {
        None
    }
}

// A network rule's pattern and its comma-separated option list. The last `$`
// is the separator, because a pattern may contain one and the options may not.
// A line whose only `$` is inside its pattern splits into an option list that
// holds no option, which every caller below reads as "not my rule" and leaves
// alone.
fn options(line: &str) -> Option<(&str, &str)> {
    line.rsplit_once('$')
}

// The substitute a `$redirect`, `$redirect-rule`, or `$rewrite` rule names, or
// None for a rule that names none. A name may carry a `:priority` suffix,
// which orders two rules naming different substitutes for the same request and
// is not part of the name.
fn substituted_name(line: &str) -> Option<&str> {
    let (_, list) = options(line)?;
    let value = list.split(',').find_map(|option| {
        let (key, value) = option.split_once('=')?;
        // An empty value names nothing. That is a malformed rule rather than a
        // substitute this build is missing, so it is left to the validator.
        (!value.is_empty() && matches!(key, "redirect" | "redirect-rule" | "rewrite"))
            .then_some(value)
    })?;
    let Some((name, priority)) = value.rsplit_once(':') else {
        return Some(value);
    };
    if priority.parse::<i32>().is_ok() {
        Some(name)
    } else {
        // `abp-resource:blank-mp4` is one name with a colon in it.
        Some(value)
    }
}

// EasyList writes `$redirect=`; uBO and AdGuard also accept `$rewrite=` for
// the same thing, and eight of the rules Omaweb ships use it. adblock-rust
// knows only the first spelling and rejects the option outright, which would
// throw away the whole rule rather than the option.
//
// Returns the rewritten rule, or None for a line that never said `rewrite=`.
fn normalize_rewrite_option(line: &str) -> Option<String> {
    let (pattern, list) = options(line)?;
    if !list.split(',').any(|option| option.starts_with("rewrite=")) {
        return None;
    }
    let rewritten: Vec<String> = list
        .split(',')
        .map(|option| match option.strip_prefix("rewrite=") {
            Some(value) => format!("redirect={value}"),
            None => option.to_owned(),
        })
        .collect();
    Some(format!("{pattern}${}", rewritten.join(",")))
}

// A $popup rule says nothing about the request beyond what kind of request it
// is, so dropping the option leaves a rule that matches the same address with
// the same domain, party, and exception conditions. What it can no longer say
// is "popups only", which is why the result is kept apart from ordinary
// requests. A $~popup rule says "anything but a popup", and an engine that is
// never asked about popups sees only such requests, so that one loses the
// option and stays where it was.
//
// Returns the rewritten rule and whether it is the popups-only kind, or None
// for a line that never mentioned popups at all.
fn split_popup_option(line: &str) -> Option<(String, bool)> {
    let (pattern, list) = options(line)?;
    let mut kept = Vec::new();
    let mut popups_only = None;
    for option in list.split(',') {
        match option {
            "popup" => popups_only = Some(true),
            "~popup" => popups_only = Some(false),
            _ => kept.push(option),
        }
    }
    let popups_only = popups_only?;
    let kept = kept.join(",");
    Some((
        if kept.is_empty() {
            pattern.to_owned()
        } else {
            format!("{pattern}${kept}")
        },
        popups_only,
    ))
}

fn stylesheet(selectors: impl IntoIterator<Item = String>) -> String {
    // Sorted so the same page yields the same stylesheet twice: the engine
    // returns hash sets, and a stylesheet that reorders itself between two
    // injections looks like a change to anything comparing them.
    let ordered: BTreeSet<String> = selectors.into_iter().collect();
    ordered
        .into_iter()
        .map(|selector| format!("{selector} {{ display: none !important; }}"))
        .collect::<Vec<_>>()
        .join("\n")
}

fn names(value: *const c_char) -> Vec<String> {
    input(value)
        .and_then(|value| serde_json::from_str::<Vec<String>>(&value).ok())
        .unwrap_or_default()
}

#[unsafe(no_mangle)]
/// # Safety
/// `rules` must be a valid NUL-terminated UTF-8 string. If non-null, `report` must be writable.
pub unsafe extern "C" fn omaweb_blocker_compile(
    rules: *const c_char,
    report: *mut *mut c_char,
) -> *mut OmawebBlocker {
    catch_unwind(AssertUnwindSafe(|| {
        let Some(rules) = input(rules) else {
            return std::ptr::null_mut();
        };
        let mut accepted = Vec::new();
        let mut popups = Vec::new();
        let mut popup_rule_count = 0;
        let mut unsupported = BTreeMap::<&str, usize>::new();
        let mut invalid_rule_count = 0;
        for line in rules.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with('!') || line.starts_with('[') {
                continue;
            }
            if let Some(category) = unsupported_category(line) {
                *unsupported.entry(category).or_default() += 1;
                continue;
            }
            let line = normalize_rewrite_option(line).unwrap_or_else(|| line.to_owned());
            let (rule, popups_only) =
                split_popup_option(&line).unwrap_or_else(|| (line.clone(), false));
            let mut validator = FilterSet::new(false);
            if validator
                .add_filter(&rule, ParseOptions::default())
                .is_err()
            {
                invalid_rule_count += 1;
                continue;
            }
            if popups_only {
                popup_rule_count += 1;
                popups.push(rule);
                continue;
            }
            // An allowlist rule that never says "popup" still allows the
            // windows a page opens, the way it allows every other request the
            // page makes. Leaving it out of the popup engine would let a list
            // refuse the payment or login window that the same list's own
            // exception was written to let through.
            if rule.starts_with("@@") {
                popups.push(rule.clone());
            }
            accepted.push(rule);
        }
        let mut engine = Engine::from_rules(&accepted, ParseOptions::default());
        engine.use_resource_storage(VendoredResources);
        let popup_engine = Engine::from_rules(&popups, ParseOptions::default());
        if !report.is_null() {
            let value = json!({
                "acceptedRuleCount": accepted.len() + popup_rule_count,
                "invalidRuleCount": invalid_rule_count,
                "unsupported": unsupported,
            });
            unsafe { *report = output(value.to_string()) };
        }
        Box::into_raw(Box::new(OmawebBlocker {
            engine,
            popups: popup_engine,
        }))
    }))
    .unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be null or a pointer returned by `omaweb_blocker_compile` that has not been freed.
pub unsafe extern "C" fn omaweb_blocker_destroy(blocker: *mut OmawebBlocker) {
    if !blocker.is_null() {
        let _ = catch_unwind(AssertUnwindSafe(|| unsafe {
            drop(Box::from_raw(blocker));
        }));
    }
}

/// What the lists have to say about one request. Three answers rather than
/// one, because they are independent: a `$redirect-rule` names a substitute
/// that is served only when some other rule blocks, and a `$removeparam` rule
/// strips tracking parameters off a request that is going out either way.
#[repr(C)]
pub struct OmawebBlockerDecision {
    /// A blocking rule matched and no exception took it back.
    pub blocked: bool,
    /// The name of the substitute to serve instead, or null when the request
    /// is simply refused. Never set without `blocked`.
    pub substitute: *mut c_char,
    /// The request address with tracking parameters removed, or null when no
    /// rule changed it. Never set together with `blocked`.
    pub rewritten_url: *mut c_char,
}

impl OmawebBlockerDecision {
    // No rule was consulted: a null argument, an address the engine cannot
    // parse, or a panic. The lists said nothing, which is not the same as
    // their having said "allow", but it is what a caller does with it.
    fn unanswered() -> Self {
        Self {
            blocked: false,
            substitute: std::ptr::null_mut(),
            rewritten_url: std::ptr::null_mut(),
        }
    }
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be a live matcher. String arguments must be valid NUL-terminated UTF-8, and
/// `decision` must point at writable storage for one `OmawebBlockerDecision`. The strings it
/// comes back holding belong to the caller until `omaweb_blocker_decision_release` frees them.
pub unsafe extern "C" fn omaweb_blocker_check(
    blocker: *const OmawebBlocker,
    url: *const c_char,
    source_url: *const c_char,
    resource_type: *const c_char,
    decision: *mut OmawebBlockerDecision,
) {
    if decision.is_null() {
        return;
    }
    let answer = catch_unwind(AssertUnwindSafe(|| {
        let (Some(blocker), Some(url), Some(source_url), Some(resource_type)) = (
            unsafe { blocker.as_ref() },
            input(url),
            input(source_url),
            input(resource_type),
        ) else {
            return OmawebBlockerDecision::unanswered();
        };
        let Ok(request) = Request::new(&url, &source_url, &resource_type) else {
            return OmawebBlockerDecision::unanswered();
        };
        let result = blocker.engine.check_network_request(&request);
        // A redirect does not imply a block: a `redirect-rule` names a
        // substitute that stands in only once a separate blocking rule has
        // matched, and `removeparam` rewrites an address the request is still
        // going out to. Each answer is dropped where it would mean nothing, so
        // that a caller acting on one cannot act on it at the wrong moment.
        OmawebBlockerDecision {
            blocked: result.matched,
            // The engine reports the body it chose rather than the name the
            // rule asked for, and the name is what Omaweb serves it under.
            substitute: result
                .redirect
                .as_deref()
                .filter(|_| result.matched)
                .and_then(|body| LIBRARY.substitute_name(body))
                .map_or(std::ptr::null_mut(), |name| output(name.to_owned())),
            rewritten_url: result
                .rewritten_url
                .filter(|_| !result.matched)
                .map_or(std::ptr::null_mut(), output),
        }
    }))
    .unwrap_or_else(|_| OmawebBlockerDecision::unanswered());
    unsafe { decision.write(answer) };
}

#[unsafe(no_mangle)]
/// # Safety
/// `decision` must be null or point at a `OmawebBlockerDecision` filled in by `omaweb_blocker_check`
/// whose strings have not already been freed.
pub unsafe extern "C" fn omaweb_blocker_decision_release(decision: *mut OmawebBlockerDecision) {
    let Some(decision) = (unsafe { decision.as_mut() }) else {
        return;
    };
    for value in [&mut decision.substitute, &mut decision.rewritten_url] {
        if !value.is_null() {
            let _ = catch_unwind(AssertUnwindSafe(|| unsafe {
                drop(CString::from_raw(*value));
            }));
            *value = std::ptr::null_mut();
        }
    }
}

#[unsafe(no_mangle)]
/// # Safety
/// `name` must be a valid NUL-terminated UTF-8 string.
///
/// Returns the substitute body this build carries under `name`, as a `data:` URL carrying the
/// resource's own MIME type, or null for a name it carries none for. The library is a constant
/// built into this binary, so this answers without a compiled rule set.
pub unsafe extern "C" fn omaweb_blocker_substitute(name: *const c_char) -> *mut c_char {
    catch_unwind(AssertUnwindSafe(|| {
        let Some(name) = input(name) else {
            return std::ptr::null_mut();
        };
        LIBRARY
            .substitute(&name)
            .map_or(std::ptr::null_mut(), |body| output(body.to_owned()))
    }))
    .unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be a live matcher. String arguments must be valid NUL-terminated UTF-8.
///
/// Reports whether a page opening a window for `url` is one the lists refuse. The opener is the
/// source, which is what makes `third-party` and `domain=` mean here what they mean anywhere
/// else. `other` is the request type because a $popup rule names no type of its own, and that is
/// the type every rule without one accepts.
pub unsafe extern "C" fn omaweb_blocker_matches_popup(
    blocker: *const OmawebBlocker,
    url: *const c_char,
    opener_url: *const c_char,
) -> bool {
    catch_unwind(AssertUnwindSafe(|| {
        let (Some(blocker), Some(url), Some(opener_url)) =
            (unsafe { blocker.as_ref() }, input(url), input(opener_url))
        else {
            return false;
        };
        Request::new(&url, &opener_url, "other")
            .map(|request| blocker.popups.check_network_request(&request).matched)
            .unwrap_or(false)
    }))
    .unwrap_or(false)
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be a live matcher and `url` must be a valid NUL-terminated UTF-8 string.
///
/// Returns the stylesheet for the rules written against this page's hostname. Generic rules are
/// not included: the page surveys its own classes and ids and asks for those separately.
pub unsafe extern "C" fn omaweb_blocker_cosmetic_css(
    blocker: *const OmawebBlocker,
    url: *const c_char,
) -> *mut c_char {
    catch_unwind(AssertUnwindSafe(|| {
        let (Some(blocker), Some(url)) = (unsafe { blocker.as_ref() }, input(url)) else {
            return std::ptr::null_mut();
        };
        let resources = blocker.engine.url_cosmetic_resources(&url);
        output(stylesheet(resources.hide_selectors))
    }))
    .unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be a live matcher and `url` must be a valid NUL-terminated UTF-8 string.
///
/// Returns the JavaScript the `##+js(...)` rules for this page ask to run: the source of each
/// named function from the vendored library, its dependencies, and the calls with the rules'
/// arguments. Empty when no rule names a scriptlet here, when an `#@#+js(...)` exception takes
/// one back, or when the named resource is one uBO gates behind trust.
pub unsafe extern "C" fn omaweb_blocker_scriptlet_source(
    blocker: *const OmawebBlocker,
    url: *const c_char,
) -> *mut c_char {
    catch_unwind(AssertUnwindSafe(|| {
        let (Some(blocker), Some(url)) = (unsafe { blocker.as_ref() }, input(url)) else {
            return std::ptr::null_mut();
        };
        output(blocker.engine.url_cosmetic_resources(&url).injected_script)
    }))
    .unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be a live matcher and `url` must be a valid NUL-terminated UTF-8 string.
///
/// Reports whether this page still needs to survey its classes and ids. A `$generichide`
/// exception turns the survey off for the whole page.
pub unsafe extern "C" fn omaweb_blocker_cosmetic_survey_wanted(
    blocker: *const OmawebBlocker,
    url: *const c_char,
) -> bool {
    catch_unwind(AssertUnwindSafe(|| {
        let (Some(blocker), Some(url)) = (unsafe { blocker.as_ref() }, input(url)) else {
            return false;
        };
        !blocker.engine.url_cosmetic_resources(&url).generichide
    }))
    .unwrap_or(false)
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be a live matcher. String arguments must be valid NUL-terminated UTF-8, and
/// `classes` and `ids` must be JSON arrays of strings.
///
/// Returns the stylesheet for the generic rules that the classes and ids actually on the page
/// could trigger. Shipping every generic rule instead cost a 617 KB stylesheet on every page.
pub unsafe extern "C" fn omaweb_blocker_generic_cosmetic_css(
    blocker: *const OmawebBlocker,
    url: *const c_char,
    classes: *const c_char,
    ids: *const c_char,
) -> *mut c_char {
    catch_unwind(AssertUnwindSafe(|| {
        let (Some(blocker), Some(url)) = (unsafe { blocker.as_ref() }, input(url)) else {
            return std::ptr::null_mut();
        };
        let resources = blocker.engine.url_cosmetic_resources(&url);
        if resources.generichide {
            return output(String::new());
        }
        let selectors = blocker.engine.hidden_class_id_selectors(
            names(classes),
            names(ids),
            &resources.exceptions,
        );
        output(stylesheet(selectors))
    }))
    .unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
/// # Safety
/// `value` must be null or a pointer returned by this library that has not been freed.
pub unsafe extern "C" fn omaweb_blocker_string_free(value: *mut c_char) {
    if !value.is_null() {
        let _ = catch_unwind(AssertUnwindSafe(|| unsafe {
            drop(CString::from_raw(value));
        }));
    }
}
