// Turns the vendored uBlock Origin scriptlet modules into the resource
// descriptors adblock-rust's `Engine::use_resources` accepts, and writes them
// to scriptlets.json next to them.
//
// uBO's scriptlets are ES modules that register themselves with
// `registerScriptlet(fn, details)`, so the only honest way to read the set is
// to import the modules and ask. That is why this step is JavaScript and why
// its output is committed: Node is a vendoring tool here, not a build
// dependency. scripts/sync_ubo_scriptlets.py drives it.
//
// A scriptlet uBO marks `requiresTrust` is one it only lets a list the user
// vouched for inject — `trusted-set-cookie` can set any cookie to any value.
// Omaweb grants no list that trust, so those resources are pinned behind a
// permission bit that nothing here ever holds, and adblock-rust refuses them.
// See docs/adr/0025-run-only-vendored-scriptlets.md.

import { createHash } from 'node:crypto'
import fs from 'node:fs'
import path from 'node:path'
import url from 'node:url'

const TRUST_REQUIRED = 0b00000001

const vendorRoot = path.resolve(
    import.meta.dirname, '..', 'third_party', 'ubo-scriptlets')
const moduleRoot = path.join(vendorRoot, 'src', 'js', 'resources')
const output = path.join(vendorRoot, 'scriptlets.json')

const importable = fs.readdirSync(moduleRoot)
    .filter(name => name.endsWith('.js'))
    .sort()
for (const name of importable) {
    await import(url.pathToFileURL(path.join(moduleRoot, name)).href)
}
const { registeredScriptlets } = await import(
    url.pathToFileURL(path.join(moduleRoot, 'base.js')).href)

// A `.fn` resource is a shared function or class other scriptlets depend on;
// adblock-rust emits its source ahead of the call and never injects it alone.
const mime = name => name.endsWith('.fn') ? 'fn/javascript' : 'application/javascript'

// adblock-rust calls a scriptlet by the name in its source, so a resource a
// filter can name has to be a plain function declaration. A `.fn` dependency
// is emitted verbatim and may be a class.
const CALLABLE = /^function\s+([^(){}\s]+)\s*\(/

const problems = []
const resources = []
for (const details of registeredScriptlets) {
    const source = String(details.fn)
    if (!details.name.endsWith('.fn') && !CALLABLE.test(source)) {
        problems.push(`${details.name} is not a callable function declaration`)
        continue
    }
    resources.push({
        name: details.name,
        aliases: details.aliases ?? [],
        kind: {mime: mime(details.name)},
        content: Buffer.from(source, 'utf8').toString('base64'),
        dependencies: details.dependencies ?? [],
        permission: details.requiresTrust ? TRUST_REQUIRED : 0,
    })
}

const named = new Set()
for (const resource of resources) {
    for (const identifier of [resource.name, ...resource.aliases]) {
        if (named.has(identifier)) {
            problems.push(`${identifier} is registered twice`)
        }
        named.add(identifier)
    }
}
for (const resource of resources) {
    for (const dependency of resource.dependencies) {
        if (!named.has(dependency)) {
            problems.push(`${resource.name} depends on unknown ${dependency}`)
        }
    }
}
if (problems.length > 0) {
    console.error('Cannot build the scriptlet library:')
    for (const problem of problems) {
        console.error(`  ${problem}`)
    }
    process.exit(1)
}

resources.sort((left, right) => left.name < right.name ? -1 : 1)
const serialized = JSON.stringify(resources, null, 2) + '\n'
fs.writeFileSync(output, serialized)

const trusted = resources.filter(resource => resource.permission !== 0).length
console.log(`${resources.length} resources (${trusted} trust-gated) -> `
    + path.relative(process.cwd(), output))
console.log(createHash('sha256').update(serialized).digest('hex'))
