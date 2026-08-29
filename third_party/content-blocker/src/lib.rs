use adblock::{
    Engine,
    lists::{FilterSet, ParseOptions},
    request::Request,
};
use serde_json::json;
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::{CStr, CString, c_char};
use std::panic::{AssertUnwindSafe, catch_unwind};

struct CosmeticRule {
    domains: Vec<(String, bool)>,
    selector: String,
    exception: bool,
}

pub struct TantoBlocker {
    engine: Engine,
    cosmetic_rules: Vec<CosmeticRule>,
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

fn unsupported_category(line: &str) -> Option<&'static str> {
    let trimmed = line.trim();
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
    if trimmed.contains("##+js(") || trimmed.contains("#@#+js(") {
        Some("scriptlets")
    } else if PROCEDURAL_MARKERS
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
        && !trimmed.contains("##")
    {
        Some("dynamic rules")
    } else {
        None
    }
}

fn parse_cosmetic(line: &str) -> Option<CosmeticRule> {
    let (separator, exception) = if line.contains("#@#") {
        ("#@#", true)
    } else if line.contains("##") {
        ("##", false)
    } else {
        return None;
    };
    let (domain_text, selector) = line.split_once(separator)?;
    let selector = selector.trim();
    if selector.is_empty() {
        return None;
    }
    let domains = domain_text
        .split(',')
        .filter(|value| !value.is_empty())
        .map(|value| {
            let value = value.trim();
            let excluded = value.starts_with('~');
            (value.trim_start_matches('~').to_ascii_lowercase(), excluded)
        })
        .collect();
    Some(CosmeticRule {
        domains,
        selector: selector.to_owned(),
        exception,
    })
}

fn applies_to(rule: &CosmeticRule, host: &str) -> bool {
    if rule.domains.is_empty() {
        return true;
    }
    let matches = |domain: &str| host == domain || host.ends_with(&format!(".{domain}"));
    if rule
        .domains
        .iter()
        .any(|(domain, excluded)| *excluded && matches(domain))
    {
        return false;
    }
    let included: Vec<_> = rule
        .domains
        .iter()
        .filter(|(_, excluded)| !excluded)
        .collect();
    included.is_empty() || included.iter().any(|(domain, _)| matches(domain))
}

fn hostname(url: &str) -> String {
    url.split_once("://")
        .map(|(_, rest)| rest)
        .unwrap_or(url)
        .split(['/', ':', '?', '#'])
        .next()
        .unwrap_or_default()
        .trim_end_matches('.')
        .to_ascii_lowercase()
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
        let mut cosmetic_rules = Vec::new();
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
            let mut validator = FilterSet::new(false);
            if validator.add_filter(line, ParseOptions::default()).is_err() {
                invalid_rule_count += 1;
                continue;
            }
            if let Some(rule) = parse_cosmetic(line) {
                cosmetic_rules.push(rule);
            }
            accepted.push(line);
        }
        let engine = Engine::from_rules(&accepted, ParseOptions::default());
        if !report.is_null() {
            let value = json!({
                "acceptedRuleCount": accepted.len(),
                "invalidRuleCount": invalid_rule_count,
                "unsupported": unsupported,
            });
            unsafe { *report = output(value.to_string()) };
        }
        Box::into_raw(Box::new(TantoBlocker {
            engine,
            cosmetic_rules,
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
/// `blocker` must be a live matcher and `url` must be a valid NUL-terminated UTF-8 string.
pub unsafe extern "C" fn tanto_blocker_cosmetic_css(
    blocker: *const TantoBlocker,
    url: *const c_char,
) -> *mut c_char {
    catch_unwind(AssertUnwindSafe(|| {
        let (Some(blocker), Some(url)) = (unsafe { blocker.as_ref() }, input(url)) else {
            return std::ptr::null_mut();
        };
        let host = hostname(&url);
        let mut hidden = BTreeSet::new();
        let mut excepted = BTreeSet::new();
        for rule in &blocker.cosmetic_rules {
            if applies_to(rule, &host) {
                if rule.exception {
                    excepted.insert(rule.selector.as_str());
                } else {
                    hidden.insert(rule.selector.as_str());
                }
            }
        }
        hidden.retain(|selector| !excepted.contains(selector));
        output(
            hidden
                .into_iter()
                .map(|selector| format!("{selector} {{ display: none !important; }}"))
                .collect::<Vec<_>>()
                .join("\n"),
        )
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
