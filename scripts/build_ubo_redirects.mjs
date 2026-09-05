// Turns the vendored uBlock Origin web-accessible resources into the resource
// descriptors adblock-rust's `Engine::use_resources` accepts, and writes them
// to redirects.json next to them.
//
// A `$redirect=` rule names one of these bodies, and the engine hands it back
// in place of the request the lists refuse. Which files are nameable is
// upstream's `redirect-resources.js`, an ES module exporting a Map, so the
// honest way to read the set is to import it rather than to list the
// directory: the directory also holds uBO's own extension pages, which no
// filter may name. scripts/sync_ubo_scriptlets.py drives this.
//
// The rules below mirror adblock-rust's own `resource_assembler`, which reads
// the same two inputs — a resource whose entry declares `params` is a
// template the engine has no way to fill, so it is left out rather than
// shipped as a body that would be served with `{{1}}` still in it.

import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import url from "node:url";

const vendorRoot = path.resolve(import.meta.dirname, "..", "third_party", "ubo-scriptlets");
const bodyRoot = path.join(vendorRoot, "src", "web_accessible_resources");
const mapPath = path.join(vendorRoot, "src", "js", "redirect-resources.js");
const output = path.join(vendorRoot, "redirects.json");

// adblock-rust reads a resource's MIME type off its extension and accepts
// only this set; anything else it calls application/octet-stream, which is
// what a browser then refuses to run. A body whose extension is not here is
// reported rather than silently shipped as one that cannot be served.
const MIME = {
  css: "text/css",
  gif: "image/gif",
  html: "text/html",
  js: "application/javascript",
  json: "application/json",
  mp3: "audio/mp3",
  mp4: "video/mp4",
  png: "image/png",
  txt: "text/plain",
  xml: "text/xml",
};

// Textual bodies are normalised to LF, the way upstream's own assembler does,
// so a checkout with CRLF line endings cannot change the digest.
const TEXTUAL = new Set(["application/javascript", "text/html", "text/plain"]);

const redirects = (await import(url.pathToFileURL(mapPath).href)).default;

const problems = [];
const resources = [];
for (const [name, details] of redirects) {
  // A parameterised resource is filled in by uBO's own click2load page.
  // adblock-rust has nowhere to take the parameters from.
  if (details.params) {
    continue;
  }
  const extension = name.includes(".") ? name.slice(name.lastIndexOf(".") + 1) : "";
  const mime = MIME[extension];
  const body = path.join(bodyRoot, name);
  if (!fs.existsSync(body)) {
    problems.push(`${name} is named by redirect-resources.js but is not vendored`);
    continue;
  }
  // `empty` is the one nameable body with no extension, and a zero-length
  // one: it is served for its emptiness, so the MIME type it carries is
  // beside the point.
  if (!mime && name !== "empty") {
    problems.push(`${name} has no MIME type adblock-rust can serve`);
    continue;
  }
  const contents = fs.readFileSync(body);
  const content = TEXTUAL.has(mime)
    ? Buffer.from(contents.toString("utf8").replaceAll("\r", ""), "utf8")
    : contents;
  const alias =
    details.alias === undefined
      ? []
      : Array.isArray(details.alias)
        ? details.alias
        : [details.alias];
  resources.push({
    name,
    aliases: alias,
    kind: { mime: mime ?? "application/octet-stream" },
    content: content.toString("base64"),
    dependencies: [],
    permission: 0,
  });
}

const named = new Set();
for (const resource of resources) {
  for (const identifier of [resource.name, ...resource.aliases]) {
    if (named.has(identifier)) {
      problems.push(`${identifier} is registered twice`);
    }
    named.add(identifier);
  }
}

if (problems.length > 0) {
  console.error("Cannot build the redirect resource library:");
  for (const problem of problems) {
    console.error(`  ${problem}`);
  }
  process.exit(1);
}

resources.sort((left, right) => (left.name < right.name ? -1 : 1));
const serialized = JSON.stringify(resources, null, 2) + "\n";
fs.writeFileSync(output, serialized);

console.log(`${resources.length} redirect resources -> ` + path.relative(process.cwd(), output));
console.log(createHash("sha256").update(serialized).digest("hex"));
