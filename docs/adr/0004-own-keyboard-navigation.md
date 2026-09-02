# Own keyboard navigation

Omaweb implements Keyboard navigation as a first-party built-in capability controlled by a setting rather than depending on Vimium or another engine-specific extension. Native commands control the browser interface, while an injected page script handles scrolling, link hints, and page commands. Each engine adapter must provide the same command contract.

Keyboard navigation has no persistent Normal or Insert state. It ignores editable controls, supports per-site key-routing overrides, and lets a site receive all keys when needed. Bindings live in versioned JSON. The interface identifies consumed key sequences so hidden state does not trap the user.

The default page commands include Vimium-style link hints. `f` labels click targets and activates the selected target in the current tab. `Shift+F` uses the same target selection and opens the result in a background tab. Target discovery and hint ordering are part of the shared engine contract.
