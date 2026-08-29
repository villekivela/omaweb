(() => {
    if (globalThis.__tantoKeyboardNavigation) return;
    const state = {
        config: { enabled: false, bindings: {}, passthroughAll: false, passthroughKeys: [] },
        prefix: '', prefixTimer: 0, hintInput: '', hints: [], hintMode: ''
    };
    const keyName = event => event.shiftKey && event.key.toLowerCase() === 'f'
        ? 'Shift+F' : event.key;
    const editable = target => target && (target.isContentEditable
        || (typeof target.closest === 'function'
            && target.closest('input, textarea, select, [contenteditable="true"]')));
    const visible = element => {
        const style = getComputedStyle(element);
        const rect = element.getBoundingClientRect();
        return style.visibility !== 'hidden' && style.display !== 'none'
            && rect.width > 0 && rect.height > 0
            && rect.bottom >= 0 && rect.right >= 0
            && rect.top <= innerHeight && rect.left <= innerWidth;
    };
    const labelFor = index => {
        const alphabet = 'asdfghjklqwertyuiopzxcvbnm';
        let label = '';
        do {
            label = alphabet[index % alphabet.length] + label;
            index = Math.floor(index / alphabet.length) - 1;
        } while (index >= 0);
        return label;
    };
    const clearHints = () => {
        document.getElementById('__tanto_link_hints')?.remove();
        state.hints = [];
        state.hintInput = '';
        state.hintMode = '';
    };
    const clearPrefix = () => {
        state.prefix = '';
        clearTimeout(state.prefixTimer);
        document.getElementById('__tanto_key_sequence')?.remove();
    };
    const showPrefix = key => {
        document.getElementById('__tanto_key_sequence')?.remove();
        const indicator = document.createElement('span');
        indicator.id = '__tanto_key_sequence';
        indicator.textContent = key + '…';
        indicator.setAttribute('role', 'status');
        indicator.setAttribute('aria-label', 'Keyboard sequence ' + key);
        indicator.style.cssText = 'position:fixed;right:1rem;bottom:1rem;z-index:2147483647;'
            + 'padding:.25em .5em;border:2px solid ButtonText;border-radius:.25em;'
            + 'background:ButtonFace;color:ButtonText;font:700 max(12px,.8rem) system-ui,sans-serif;';
        document.documentElement.append(indicator);
    };
    const showHints = command => {
        clearHints();
        let candidates = [...document.querySelectorAll(
            'a[href], button, input:not([type="hidden"]), select, textarea, [role="link"], [role="button"], [tabindex]')]
            .filter(visible);
        if (command === 'open-link-background') {
            candidates = candidates.filter(target => Boolean(target.href));
        }
        const overlay = document.createElement('div');
        overlay.id = '__tanto_link_hints';
        overlay.setAttribute('role', 'status');
        overlay.setAttribute('aria-live', 'polite');
        overlay.setAttribute('aria-label', candidates.length + ' link hints available');
        const rootStyle = getComputedStyle(document.documentElement);
        overlay.style.cssText = 'position:fixed;inset:0;pointer-events:none;z-index:2147483647;'
            + 'color:' + rootStyle.color + ';font:700 max(12px,.8rem) system-ui,sans-serif;';
        state.hints = candidates.map((target, index) => {
            const label = labelFor(index);
            const rect = target.getBoundingClientRect();
            const hint = document.createElement('span');
            hint.textContent = label;
            hint.setAttribute('role', 'note');
            hint.setAttribute('aria-label', 'Link hint ' + label + ' for '
                + (target.getAttribute('aria-label') || target.textContent || target.href || 'target').trim());
            hint.style.cssText = 'position:absolute;left:' + Math.max(0, rect.left) + 'px;top:'
                + Math.max(0, rect.top) + 'px;padding:.15em .35em;border:2px solid ButtonText;'
                + 'border-radius:.25em;background:ButtonFace;color:ButtonText;line-height:1.2;'
                + 'forced-color-adjust:auto;box-shadow:0 1px 3px #0008;';
            overlay.append(hint);
            return { label, target, hint };
        });
        document.documentElement.append(overlay);
        state.hintMode = command;
    };
    const activateHint = entry => {
        const target = entry.target;
        const background = state.hintMode === 'open-link-background';
        clearHints();
        if (background) {
            target.dispatchEvent(new MouseEvent('click', {
                bubbles: true, cancelable: true, view: window,
                button: 0, ctrlKey: true, metaKey: true
            }));
        } else {
            target.click();
        }
    };
    const execute = command => {
        const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;
        const behavior = reduced ? 'instant' : 'smooth';
        if (command === 'scroll-down') scrollBy({ top: Math.max(48, innerHeight * .5), behavior });
        else if (command === 'scroll-up') scrollBy({ top: -Math.max(48, innerHeight * .5), behavior });
        else if (command === 'scroll-top') scrollTo({ top: 0, behavior });
        else if (command === 'scroll-bottom') scrollTo({ top: document.documentElement.scrollHeight, behavior });
        else if (command === 'open-link' || command === 'open-link-background') showHints(command);
    };
    addEventListener('keydown', event => {
        if (!state.config.enabled || event.defaultPrevented || event.isComposing
                || event.ctrlKey || event.metaKey || event.altKey || editable(event.target)) return;
        const key = keyName(event);
        if (state.config.passthroughAll || state.config.passthroughKeys.includes(key)) return;
        if (state.hintMode) {
            if (event.key === 'Escape') {
                event.preventDefault();
                clearHints();
                return;
            }
            if (event.key.length !== 1) return;
            event.preventDefault();
            event.stopImmediatePropagation();
            state.hintInput += event.key.toLowerCase();
            const matches = state.hints.filter(item => item.label.startsWith(state.hintInput));
            state.hints.forEach(item => item.hint.style.opacity = matches.includes(item) ? '1' : '.25');
            if (matches.length === 1 && matches[0].label === state.hintInput) activateHint(matches[0]);
            else if (!matches.length) clearHints();
            return;
        }
        const bindings = state.config.bindings || {};
        if (state.prefix) {
            const sequence = state.prefix + key;
            clearPrefix();
            if (bindings[sequence]) {
                event.preventDefault();
                event.stopImmediatePropagation();
                execute(bindings[sequence]);
                return;
            }
        }
        const hasSequence = Object.keys(bindings).some(binding => binding.length > key.length
            && binding.startsWith(key));
        if (hasSequence) {
            event.preventDefault();
            event.stopImmediatePropagation();
            state.prefix = key;
            showPrefix(key);
            state.prefixTimer = setTimeout(clearPrefix, 700);
            return;
        }
        if (!bindings[key]) return;
        event.preventDefault();
        event.stopImmediatePropagation();
        execute(bindings[key]);
    }, true);
    globalThis.__tantoKeyboardNavigation = {
        configure(configuration) {
            clearHints();
            clearPrefix();
            state.config = configuration;
        }
    };
})();
