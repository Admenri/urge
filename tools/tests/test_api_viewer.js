// Backend-free browser viewer regressions; no network or DOM package required.
// Run: node --test tools/tests/test_api_viewer.js

"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");

const root = path.join(__dirname, "../..");
const html = fs.readFileSync(path.join(root, "bind/viewer/index.html"), "utf8");
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)];
assert.equal(scripts.length, 1);
const source = scripts[0][1].replace(/\nboot\(\);\s*$/, "\n");
assert.notEqual(source, scripts[0][1], "Disable automatic fetching in this test only");

function fixture() {
  return {
    classes: [{
      name: "Socket", kind: "class", cpp_name: "Socket",
      constants: [
        { name: "OPEN", cpp_name: "State::Open", value: "0" },
        { name: "NEXT", cpp_name: "State::Next", value: null },
      ],
      instance_methods: [{ ruby_name: "read", cpp_name: "Read", params: [] }],
    }],
    modules: [{
      name: "GPU", kind: "module",
      constants: [{ name: "MASK", cpp_name: "Mask", value: "1ULL << 63", doc: "Wide unsigned mask" }],
      functions: [{ ruby_name: "read", cpp_name: "Read", params: [] }],
    }],
  };
}

function viewer(ir = fixture()) {
  function element() {
    return {
      innerHTML: "", value: "", children: [], dataset: {},
      addEventListener() {},
      appendChild(child) { this.children.push(child); },
      classList: { add() {}, remove() {} },
      querySelectorAll() { return []; },
    };
  }
  const elements = {
    main: element(), nav: element(), navfilter: element(), suggest: element(),
    q: element(), reload: element(), meta: element(),
  };
  const context = {
    document: {
      getElementById(id) { assert.ok(elements[id], id); return elements[id]; },
      createElement: element,
      querySelectorAll() { return []; },
      querySelector() { return null; },
      addEventListener() {},
    },
    location: { hash: "" },
    fetch() { assert.fail("Viewer tests must not make network requests"); },
  };
  context.window = context;
  vm.runInNewContext(source, context, { filename: "bind/viewer/index.html" });
  context.buildState(ir, "test", "api.json");
  context.suggestEl = elements.suggest;
  return { window: context, elements };
}

function calls(members) {
  // Materialize in this realm rather than comparing arrays across VM realms.
  return Array.from(members, (m) => m.call);
}

test("classes and modules expose constants as bare :: lookups in the search index", () => {
  const { window: page } = viewer();
  assert.deepEqual(calls(page.STATE.memberIndex), [
    "Socket::OPEN", "Socket::NEXT", "Socket#read", "GPU::MASK", "GPU.read",
  ]);
  const m = page.computeHits("Socket::OPEN")[0];
  assert.equal(page.signatureOf(m, true), "Socket::OPEN");
  assert.equal(page.signatureOf(m, false), "Socket::OPEN");
  assert.equal(page.returnType(m), "Integer");
  assert.equal(m.writer, null);
  assert.equal(m.kind, "constant");
});

test("search matches constant names, source values, C++ names, types and documentation", () => {
  const { window: page } = viewer();
  for (const [query, expected] of [
    ["Socket::OPEN", "Socket::OPEN"], ["State::Next", "Socket::NEXT"],
    ["1ULL << 63", "GPU::MASK"], ["Wide unsigned", "GPU::MASK"],
  ]) {
    assert.equal(page.computeHits(query)[0].call, expected);
  }
  assert.equal(page.computeHits("Integer").length, 3);
});

test("detail includes constants for modules and classes, correct counts and highlighted rows", () => {
  const { window: page, elements } = viewer();
  page.renderDetail("Socket", "OPEN");
  assert.match(elements.main.innerHTML, /Name<\/th><th>Type<\/th><th>Value \/ C\+\+ initializer/);
  assert.match(elements.main.innerHTML, /3 members/);
  assert.match(elements.main.innerHTML, /<tr class="flag"><td[^>]+data-sig="Socket::OPEN"/);
  assert.match(elements.main.innerHTML, /<td class="mono">0<\/td>/);
  assert.match(elements.main.innerHTML, /implicit \(see C\+\+ member\)/);
  assert.match(elements.main.innerHTML, /data-sig="Socket#read\(\)"/);
  const socketNav = elements.nav.children.find((e) => e.dataset.owner === "Socket");
  assert.match(socketNav.innerHTML, /class="cnt">3<\/span>/);

  page.renderDetail("GPU", "MASK");
  assert.match(elements.main.innerHTML, /data-sig="GPU::MASK"/);
  assert.match(elements.main.innerHTML, /1ULL &lt;&lt; 63/);
  assert.match(elements.main.innerHTML, /Wide unsigned mask/);
  assert.match(elements.main.innerHTML, /data-sig="GPU.read\(\)"/);
});

test("constant value and documentation are escaped, not evaluated or interpreted as markup", () => {
  const ir = fixture();
  ir.modules[0].constants[0].value = '<img src=x onerror="throw 1"> & (1ULL << 63)';
  ir.modules[0].constants[0].doc = "<script>alert(1)</script>";
  const { window: page, elements } = viewer(ir);
  page.renderDetail("GPU");
  assert.match(elements.main.innerHTML, /&lt;img src=x onerror=&quot;throw 1&quot;&gt; &amp;/);
  assert.match(elements.main.innerHTML, /&lt;script&gt;alert\(1\)&lt;\/script&gt;/);
  assert.doesNotMatch(elements.main.innerHTML, /<img|<script/);
});

test("search suggestions include copyable constant names and the source value", () => {
  const { window: page, elements } = viewer();
  page.renderSuggest(page.computeHits("MASK"), "MASK");
  assert.match(elements.suggest.innerHTML, /data-owner="GPU" data-name="MASK"/);
  assert.match(elements.suggest.innerHTML, /GPU::MASK/);
  assert.match(elements.suggest.innerHTML, /= 1ULL &lt;&lt; 63/);
  assert.doesNotMatch(elements.suggest.innerHTML, /GPU::MASK\(/);
});

test("boot accepts :: constant deep links for both classes and modules", async () => {
  for (const [owner, name] of [["Socket", "OPEN"], ["GPU", "MASK"]]) {
    const ir = fixture();
    const { window: page, elements } = viewer(ir);
    page.location.hash = "#" + encodeURIComponent(owner + "::" + name);
    page.loadIR = async () => ({ ir, source: "test", url: "api.json" });
    await page.boot();
    assert.equal(page.STATE.selected.owner, owner);
    assert.equal(page.STATE.selected.member, name);
    assert.match(elements.main.innerHTML, /<tr class="flag">/);
  }
});

test("older IR without constants still renders methods and searches normally", () => {
  const ir = fixture();
  ir.classes.concat(ir.modules).forEach((entry) => { delete entry.constants; });
  const { window: page, elements } = viewer(ir);
  assert.equal(page.STATE.memberIndex.length, 2);
  page.renderDetail("GPU");
  assert.match(elements.main.innerHTML, /GPU.read/);
  assert.doesNotMatch(elements.main.innerHTML, /constants-table/);
  assert.equal(page.computeHits("read").length, 2);
});

test("binary-only payload getters are described as raw ASCII-8BIT bytes", () => {
  const ir = JSON.parse(fs.readFileSync(path.join(root, "bind/api_reference.json"), "utf8"));
  const { window: page, elements } = viewer(ir);
  for (const [owner, name] of [["NetworkEvent", "data"], ["Fetch", "body"]]) {
    const member = page.computeHits(owner + "#" + name)[0];
    assert.equal(member.call, owner + "#" + name);
    assert.ok(member.flags.includes("binary"));
    assert.match(member.note, /ASCII-8BIT/);
    page.renderDetail(owner, name);
    assert.match(elements.main.innerHTML, /returns raw ASCII-8BIT bytes; no decoding/);
  }
  assert.equal(page.computeHits("Fetch#text").length, 0);
  assert.equal(page.computeHits("WebSocket#send_text").length, 0);
  assert.equal(page.computeHits("WebSocket#send")[0].cppName, "Send");
});

test("every constant in the project IR is present in the browser search index", () => {
  const ir = JSON.parse(fs.readFileSync(path.join(root, "bind/api_reference.json"), "utf8"));
  const { window: page } = viewer(ir);
  const expected = ir.classes.concat(ir.modules).reduce((n, entry) => n + (entry.constants || []).length, 0);
  assert.ok(expected > 0);
  assert.equal(page.STATE.memberIndex.filter((m) => m.kind === "constant").length, expected);
  assert.equal(page.computeHits("Input::DOWN")[0].value, "2");
  assert.equal(page.computeHits("WebSocket::OPEN")[0].value, "1");
});
