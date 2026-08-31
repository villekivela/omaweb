use adblock::{
    Engine,
    lists::{FilterSet, ParseOptions},
    request::Request,
    resources::{InMemoryResourceStorage, Resource, ResourceImpl, ResourceStorageBackend},
};
use serde_json::json;
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::{CStr, CString, c_char};
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::sync::LazyLock;

// uBlock Origin's scriptlet library, vendored and pinned under
// third_party/ubo-scriptlets and built into the binary. A `##+js(...)` rule
// contributes a name from this set and the arguments to call it with; no list
// ever supplies the code that runs.
//
// Decoded once for the process rather than per rule set: the library is a
// constant, while a rule set is recompiled whenever a subscription updates.
static SCRIPTLETS: LazyLock<Library> = LazyLock::new(|| {
    let resources: Vec<Resource> =
        serde_json::from_str(include_str!("../../ubo-scriptlets/scriptlets.json"))
            .expect("the vendored scriptlet library parses as resource descriptors");
    Library::new(resources)
});

struct Library {
    storage: InMemoryResourceStorage,
    // Which names a rule may ask for. The engine would answer this too, but
    // only once a page asks; a list is reported on when it compiles, so the
    // names are kept apart from the code.
    injectable: BTreeSet<String>,
    // The scriptlets uBO only lets a list the user vouched for inject —
    // `trusted-set-cookie` sets any cookie to any value. Tanto vouches for no
    // list, so every rule set compiles without permissions and the engine
    // refuses these. Kept apart from the unknown names so a report can say
    // which of the two happened.
    trust_gated: BTreeSet<String>,
}

impl Library {
    fn new(resources: Vec<Resource>) -> Self {
        let mut injectable = BTreeSet::new();
        let mut trust_gated = BTreeSet::new();
        for resource in &resources {
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
        Self {
            storage: InMemoryResourceStorage::from_resources(resources),
            injectable,
            trust_gated,
        }
    }

    // A rule may leave the `.js` off the name, the way the lists usually do.
    fn holds(names: &BTreeSet<String>, name: &str) -> bool {
        names.contains(name) || names.contains(&format!("{name}.js"))
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
struct VendoredScriptlets;

impl ResourceStorageBackend for VendoredScriptlets {
    fn get_resource(&self, name: &str) -> Option<ResourceImpl> {
        SCRIPTLETS.storage.get_resource(name)
    }
}

pub struct TantoBlocker {
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
        return SCRIPTLETS.refusal(name);
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
    } else if trimmed.contains("$replace") || trimmed.contains("$removeparam") {
        Some("response rewriting")
    } else if trimmed.contains("$redirect") || trimmed.contains("$rewrite") {
        Some("redirects or resource replacement")
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
    let (pattern, options) = line.rsplit_once('$')?;
    let mut kept = Vec::new();
    let mut popups_only = None;
    for option in options.split(',') {
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
pub unsafe extern "C" fn tanto_blocker_compile(
    rules: *const c_char,
    report: *mut *mut c_char,
) -> *mut TantoBlocker {
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
            let (rule, popups_only) =
                split_popup_option(line).unwrap_or_else(|| (line.to_owned(), false));
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
        engine.use_resource_storage(VendoredScriptlets);
        let popup_engine = Engine::from_rules(&popups, ParseOptions::default());
        if !report.is_null() {
            let value = json!({
                "acceptedRuleCount": accepted.len() + popup_rule_count,
                "invalidRuleCount": invalid_rule_count,
                "unsupported": unsupported,
            });
            unsafe { *report = output(value.to_string()) };
        }
        Box::into_raw(Box::new(TantoBlocker {
            engine,
            popups: popup_engine,
        }))
    }))
    .unwrap_or(std::ptr::null_mut())
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be null or a pointer returned by `tanto_blocker_compile` that has not been freed.
pub unsafe extern "C" fn tanto_blocker_destroy(blocker: *mut TantoBlocker) {
    if !blocker.is_null() {
        let _ = catch_unwind(AssertUnwindSafe(|| unsafe {
            drop(Box::from_raw(blocker));
        }));
    }
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be a live matcher. String arguments must be valid NUL-terminated UTF-8.
pub unsafe extern "C" fn tanto_blocker_matches(
    blocker: *const TantoBlocker,
    url: *const c_char,
    source_url: *const c_char,
    resource_type: *const c_char,
) -> bool {
    catch_unwind(AssertUnwindSafe(|| {
        let (Some(blocker), Some(url), Some(source_url), Some(resource_type)) = (
            unsafe { blocker.as_ref() },
            input(url),
            input(source_url),
            input(resource_type),
        ) else {
            return false;
        };
        Request::new(&url, &source_url, &resource_type)
            .map(|request| blocker.engine.check_network_request(&request).matched)
            .unwrap_or(false)
    }))
    .unwrap_or(false)
}

#[unsafe(no_mangle)]
/// # Safety
/// `blocker` must be a live matcher. String arguments must be valid NUL-terminated UTF-8.
///
/// Reports whether a page opening a window for `url` is one the lists refuse. The opener is the
/// source, which is what makes `third-party` and `domain=` mean here what they mean anywhere
/// else. `other` is the request type because a $popup rule names no type of its own, and that is
/// the type every rule without one accepts.
pub unsafe extern "C" fn tanto_blocker_matches_popup(
    blocker: *const TantoBlocker,
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
pub unsafe extern "C" fn tanto_blocker_cosmetic_css(
    blocker: *const TantoBlocker,
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
pub unsafe extern "C" fn tanto_blocker_scriptlet_source(
    blocker: *const TantoBlocker,
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
pub unsafe extern "C" fn tanto_blocker_cosmetic_survey_wanted(
    blocker: *const TantoBlocker,
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
pub unsafe extern "C" fn tanto_blocker_generic_cosmetic_css(
    blocker: *const TantoBlocker,
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
pub unsafe extern "C" fn tanto_blocker_string_free(value: *mut c_char) {
    if !value.is_null() {
        let _ = catch_unwind(AssertUnwindSafe(|| unsafe {
            drop(CString::from_raw(value));
        }));
    }
}
