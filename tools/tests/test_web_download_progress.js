// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Backend-free shell regressions. No browser, Wasm build, or network required.
// Run: node --test tools/tests/test_web_download_progress.js

"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");

const source = fs.readFileSync(
  path.join(__dirname, "../../app/urge-helper.js"), "utf8");
const MiB = 1024 * 1024;

function shell(entries = [], options = {}) {
  const status = { hidden: false, textContent: "Loading..." };
  // A resource name must never be treated as markup.
  Object.defineProperty(status, "innerHTML", {
    set() { assert.fail("Progress must use textContent, not innerHTML"); },
  });
  const spinner = { hidden: false };
  const elements = { status, spinner, canvas: {} };
  const requests = [];
  const files = new Map();
  const dependencies = new Set();

  class FakeXHR {
    constructor() {
      this.status = 200;
      this.statusText = "OK";
      this.response = new ArrayBuffer(0);
      requests.push(this);
    }
    open(method, url) {
      this.method = method;
      this.url = url;
    }
    send() {
      if (options.sendError) throw new Error("Request failed");
    }
    progress(loaded, total = 0, lengthComputable = true) {
      this.onprogress({ loaded, total, lengthComputable });
    }
    load(statusCode = 200) {
      this.status = statusCode;
      this.onload();
    }
  }

  const context = {
    asyncMappingData: Object.fromEntries(
      entries.map((entry) => [entry.path.toLowerCase(), entry])),
    console: { log() {}, error() {} },
    document: {
      baseURI: "https://example.test/game/Game.html",
      getElementById(id) { return options.noUI ? null : elements[id]; },
    },
    fetch: options.fetch,
    URL,
    ReadableStream,
    Response,
    XMLHttpRequest: FakeXHR,
    FS: {
      analyzePath(name) { return { exists: files.has(name) }; },
      mkdir() {},
    },
    FS_createDataFile(parent, name, bytes) {
      if (options.writeError) throw new Error("Filesystem full");
      files.set(parent.replace(/\/$/, "") + "/" + name, bytes);
    },
  };
  context.window = context;
  vm.runInNewContext(source, context, { filename: "urge-helper.js" });
  context.Module.addRunDependency = (name) => {
    assert.ok(!dependencies.has(name));
    dependencies.add(name);
    context.Module.monitorRunDependencies(dependencies.size);
  };
  context.Module.removeRunDependency = (name) => {
    assert.ok(dependencies.delete(name));
    context.Module.monitorRunDependencies(dependencies.size);
  };

  return {
    window: context, module: context.Module, status, spinner, requests, files,
    dependencies,
    preload() { context.Module.preRun.forEach((callback) => callback()); },
  };
}

function assertHidden(app) {
  assert.equal(app.status.hidden, true);
  assert.equal(app.spinner.hidden, true);
  assert.equal(app.dependencies.size, 0);
}

test("large preloads show filename, percentage, and byte progress immediately", () => {
  const app = shell([{ path: "Game.rgss3a", size: 200 * MiB, preload: true }]);
  app.preload();
  assert.equal(app.status.hidden, false);
  assert.equal(app.spinner.hidden, false);
  assert.match(app.status.textContent, /Game\.rgss3a: 0\.0% \(0 B \/ 200\.0 MiB\)/);

  app.requests[0].progress(50 * MiB, 200 * MiB);
  assert.match(app.status.textContent,
    /Game\.rgss3a: 25\.0% \(50\.0 MiB \/ 200\.0 MiB\)/);
  app.requests[0].load();
  assertHidden(app);
  assert.ok(app.files.has("/Game.rgss3a"));
});

test("browser transfer total takes precedence over the manifest size", () => {
  const app = shell([{ path: "Game.ini", size: 1000, preload: true }]);
  app.preload();
  app.requests[0].progress(100, 200);
  assert.match(app.status.textContent, /50\.0% \(100 B \/ 200 B\)/);
});

test("the manifest supplies the total without Content-Length", () => {
  const app = shell([{ path: "Data/Scripts.rvdata2", size: 2 * MiB, preload: true }]);
  app.preload();
  app.requests[0].progress(MiB, 0, false);
  assert.match(app.status.textContent, /50\.0% \(1\.0 MiB \/ 2\.0 MiB\)/);
});

test("unknown totals show bytes without inventing a percentage", () => {
  const app = shell([{ path: "Audio/BGM/theme.ogg", preload: true }]);
  app.preload();
  app.requests[0].progress(5 * MiB, 0, false);
  assert.match(app.status.textContent, /5\.0 MiB downloaded/);
  assert.doesNotMatch(app.status.textContent, /%|NaN|Infinity/);
});

test("empty files do not divide by zero and still finish", () => {
  const app = shell([{ path: "Empty.dat", size: 0, preload: true }]);
  app.preload();
  app.requests[0].progress(0, 0);
  assert.match(app.status.textContent, /0 B downloaded/);
  app.requests[0].load();
  assertHidden(app);
});

test("parallel preloads retain the unfinished file's progress", () => {
  const app = shell([
    { path: "Game.ini", size: 100, preload: true },
    { path: "Game.rgssad", size: 100 * MiB, preload: true },
  ]);
  app.preload();
  app.requests[0].progress(50, 100);
  app.requests[1].progress(10 * MiB, 100 * MiB);
  assert.match(app.status.textContent, /Game\.ini: 50\.0%/);
  assert.match(app.status.textContent, /Game\.rgssad: 10\.0%/);

  app.requests[0].load();
  assert.doesNotMatch(app.status.textContent, /Game\.ini/);
  assert.match(app.status.textContent, /Game\.rgssad: 10\.0%/);
  assert.equal(app.status.hidden, false);
  app.requests[1].load();
  assertHidden(app);
});

test("lazy downloads bring the UI back after startup and cached reads keep it hidden", () => {
  const app = shell([{ path: "Graphics/Huge.png", size: 4 * MiB }]);
  app.module.addRunDependency("wasm");
  app.module.removeRunDependency("wasm");
  app.preload();
  assertHidden(app);

  let callbacks = 0;
  app.window.fetchLazyAsset("graphics/huge", () => ++callbacks);
  assert.equal(app.status.hidden, false);
  app.requests[0].progress(MiB, 4 * MiB);
  assert.match(app.status.textContent, /Graphics\/Huge\.png: 25\.0%/);
  app.requests[0].load();
  assert.equal(callbacks, 1);
  assertHidden(app);

  app.window.fetchLazyAsset("Graphics/Huge.png", () => ++callbacks);
  assert.equal(callbacks, 2);
  assert.equal(app.requests.length, 1);
  assertHidden(app);
});

for (const failure of ["http", "error", "abort", "timeout"]) {
  test(`${failure} failures clear progress and allow a retry`, () => {
    const app = shell([{ path: "Audio/BGM/theme.ogg", size: MiB }]);
    let callbacks = 0;
    app.window.fetchLazyAsset("Audio/BGM/theme", () => ++callbacks);
    const xhr = app.requests[0];
    xhr.progress(MiB / 2, MiB);
    if (failure === "http") xhr.load(404);
    else xhr["on" + failure]();
    assertHidden(app);
    assert.equal(callbacks, 1);
    assert.equal(app.window.isAsyncFileCached("Audio/BGM/theme"), false);

    // Duplicate terminal events must not remove a dependency or wake twice.
    xhr.onerror();
    xhr.progress(MiB, MiB);
    assert.equal(callbacks, 1);
    assertHidden(app);

    app.window.fetchLazyAsset("Audio/BGM/theme", () => ++callbacks);
    assert.equal(app.requests.length, 2);
    assert.match(app.status.textContent, /0\.0%/);
    app.requests[1].load();
    assert.equal(callbacks, 2);
    assertHidden(app);
  });
}

test("request setup exceptions release the loading UI and dependency", () => {
  const app = shell([{ path: "Game.ini", size: 100 }], { sendError: true });
  let callbacks = 0;
  app.window.fetchLazyAsset("Game.ini", () => ++callbacks);
  assert.equal(callbacks, 1);
  assert.equal(app.window.isAsyncFileCached("Game.ini"), false);
  assertHidden(app);
});

test("filesystem write errors release the loading UI and dependency", () => {
  const app = shell([{ path: "Game.ini", size: 100 }], { writeError: true });
  let callbacks = 0;
  app.window.fetchLazyAsset("Game.ini", () => ++callbacks);
  app.requests[0].load();
  assert.equal(callbacks, 1);
  assert.equal(app.window.isAsyncFileCached("Game.ini"), false);
  assertHidden(app);
});

test("Unicode filenames and markup-like text remain literal", () => {
  const name = "Graphics/大图<&>.png";
  const app = shell([{ path: name, size: MiB, preload: true }]);
  app.preload();
  assert.ok(app.status.textContent.includes(name));
  assert.equal(app.requests[0].url, encodeURI("assets/" + name));
  app.requests[0].load(0);
  assert.ok(app.files.has("/" + name));
  assertHidden(app);
});

test("embedded Game.data progress and other Emscripten statuses reach the shell", () => {
  const app = shell();
  app.module.setStatus("Downloading data... (52428800/209715200)");
  assert.match(app.status.textContent,
    /Downloading data.*25\.0% \(50\.0 MiB \/ 200\.0 MiB\)/);
  assert.equal(app.status.hidden, false);
  app.module.setStatus("Running...");
  assert.equal(app.status.textContent, "Running...");
  app.module.setStatus("");
  assertHidden(app);
});

test("runtime status updates cannot hide an active asset download", () => {
  const app = shell([{ path: "Game.rgssad", size: MiB, preload: true }]);
  app.preload();
  app.requests[0].progress(MiB / 2, MiB);
  app.module.setStatus("Running...");
  app.module.setStatus("");
  assert.match(app.status.textContent, /Game\.rgssad: 50\.0%/);
  assert.equal(app.status.hidden, false);
  app.requests[0].load();
  assertHidden(app);
});

test("non-asset dependencies keep their preparation indicator", () => {
  const app = shell();
  app.module.addRunDependency("wasm");
  app.module.setStatus("");
  assert.equal(app.status.textContent, "Preparing... (0/1)");
  assert.equal(app.spinner.hidden, false);
  app.module.removeRunDependency("wasm");
  assertHidden(app);
});

test("no preloads and absent UI elements do not break initialization", () => {
  const app = shell();
  app.preload();
  assertHidden(app);

  const headless = shell([{ path: "Game.ini", size: 100, preload: true }],
    { noUI: true });
  headless.preload();
  headless.requests[0].progress(50, 100);
  headless.requests[0].load();
  assert.equal(headless.dependencies.size, 0);
});

function wasmShell(fetch) {
  const app = shell([], { fetch });
  app.wasmURL = app.module.locateFile("Game.wasm", "https://example.test/game/");
  return app;
}

function openResponse(headers = {}) {
  let controller;
  let cancelled;
  const response = new Response(new ReadableStream({
    start(value) { controller = value; },
    cancel(reason) { cancelled = reason; },
  }), { headers: { "Content-Type": "application/wasm", ...headers } });
  return { response, get controller() { return controller; },
    get cancelled() { return cancelled; } };
}

const nextTurn = () => new Promise(setImmediate);
const emptyWasm = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0]);

test("Game.wasm reports streamed bytes and a known total before completion", async () => {
  const input = openResponse({ "Content-Length": String(2 * MiB) });
  const app = wasmShell(async () => input.response);
  const promise = app.window.fetch(app.wasmURL, { credentials: "same-origin" });
  assert.match(app.status.textContent, /Game\.wasm: 0 B downloaded/);
  const response = await promise;
  assert.equal(response.headers.get("Content-Type"), "application/wasm");
  assert.match(app.status.textContent, /Game\.wasm: 0\.0% \(0 B \/ 2\.0 MiB\)/);

  const bytes = response.arrayBuffer();
  input.controller.enqueue(new Uint8Array(MiB));
  await nextTurn();
  assert.match(app.status.textContent, /Game\.wasm: 50\.0% \(1\.0 MiB \/ 2\.0 MiB\)/);
  assert.equal(app.spinner.hidden, false);
  input.controller.enqueue(new Uint8Array(MiB));
  input.controller.close();
  assert.equal((await bytes).byteLength, 2 * MiB);
  assertHidden(app);
});

for (const encoding of [null, "gzip", "br"]) {
  test(`Wasm with ${encoding || "no total"} shows decoded bytes without a false percentage`, async () => {
    const headers = encoding ? { "Content-Length": "100", "Content-Encoding": encoding } : {};
    const input = openResponse(headers);
    const app = wasmShell(async () => input.response);
    const response = await app.window.fetch(app.wasmURL);
    const bytes = response.arrayBuffer();
    input.controller.enqueue(new Uint8Array(MiB));
    await nextTurn();
    assert.match(app.status.textContent, /Game\.wasm: 1\.0 MiB downloaded/);
    assert.doesNotMatch(app.status.textContent, /%|NaN|Infinity/);
    input.controller.close();
    await bytes;
    assertHidden(app);
  });
}

test("locateFile preserves runtime paths and unrelated fetches remain untouched", async () => {
  const original = Promise.resolve(new Response("asset"));
  let args;
  let receiver;
  const app = wasmShell(function (...values) {
    args = values;
    receiver = this;
    return original;
  });
  assert.equal(app.wasmURL, "https://example.test/game/Game.wasm");
  assert.equal(app.module.locateFile("Game.data", "https://cdn.test/"),
    "https://cdn.test/Game.data");
  const options = { credentials: "omit" };
  const result = app.window.fetch("https://other.test/Game.wasm", options);
  assert.equal(result, original);
  assert.equal(args[0], "https://other.test/Game.wasm");
  assert.equal(args[1], options);
  assert.equal(receiver, app.window);
  assert.equal(await (await result).text(), "asset");
  assert.equal(app.status.textContent, "Loading...");
});

test("Wasm URL and Request inputs retain request options", async () => {
  const requests = [];
  const app = wasmShell(async (input, init) => {
    requests.push([input, init]);
    return new Response(emptyWasm, { headers: { "Content-Type": "application/wasm" } });
  });
  const options = { credentials: "same-origin" };
  for (const input of ["Game.wasm", new URL(app.wasmURL), new Request(app.wasmURL)]) {
    const response = await app.window.fetch(input, options);
    assert.match(app.status.textContent, /Game\.wasm/);
    assert.deepEqual(new Uint8Array(await response.arrayBuffer()), emptyWasm);
    assert.equal(requests.at(-1)[0], input);
    assert.equal(requests.at(-1)[1], options);
    assertHidden(app);
  }
});

test("Wasm fetch failures propagate and clear the transfer", async () => {
  const error = new Error("Network unavailable");
  const app = wasmShell(async () => { throw error; });
  await assert.rejects(app.window.fetch(app.wasmURL), (value) => value === error);
  assertHidden(app);
});

test("Wasm HTTP errors remain available to Emscripten's error path", async () => {
  const original = new Response("not found", { status: 404 });
  const app = wasmShell(async () => original);
  assert.equal(await app.window.fetch(app.wasmURL), original);
  assertHidden(app);
});

test("Wasm stream failures propagate and clear the transfer", async () => {
  const input = openResponse();
  const app = wasmShell(async () => input.response);
  const response = await app.window.fetch(app.wasmURL);
  const bytes = response.arrayBuffer();
  const error = new Error("Transfer interrupted");
  input.controller.error(error);
  await assert.rejects(bytes, (value) => value === error);
  assertHidden(app);
});

test("cancelling the Wasm response cancels the original stream", async () => {
  const input = openResponse();
  const app = wasmShell(async () => input.response);
  const response = await app.window.fetch(app.wasmURL);
  await response.body.cancel("stop loading");
  assert.equal(input.cancelled, "stop loading");
  assertHidden(app);
});

test("an older Wasm response cannot clear a replacement transfer", async () => {
  const inputs = [openResponse(), openResponse({ "Content-Length": String(MiB) })];
  let request = 0;
  const app = wasmShell(async () => inputs[request++].response);
  const first = await app.window.fetch(app.wasmURL);
  const second = await app.window.fetch(app.wasmURL);
  await first.body.cancel("streaming fallback");
  assert.equal(app.status.hidden, false);
  assert.match(app.status.textContent, /Game\.wasm: 0\.0%/);
  const bytes = second.arrayBuffer();
  inputs[1].controller.enqueue(new Uint8Array(MiB));
  inputs[1].controller.close();
  await bytes;
  assertHidden(app);
});

test("native WebAssembly.instantiateStreaming accepts the counted response", async () => {
  const input = openResponse({ "Content-Length": String(emptyWasm.byteLength) });
  const app = wasmShell(async () => input.response);
  const instantiation = WebAssembly.instantiateStreaming(app.window.fetch(app.wasmURL), {});
  input.controller.enqueue(emptyWasm.slice(0, 4));
  await nextTurn();
  assert.match(app.status.textContent, /Game\.wasm: 50\.0% \(4 B \/ 8 B\)/);
  input.controller.enqueue(emptyWasm.slice(4));
  input.controller.close();
  const result = await instantiation;
  assert.ok(result.instance instanceof WebAssembly.Instance);
  assertHidden(app);
});

test("a bad Wasm MIME type can still use Emscripten's ArrayBuffer fallback", async () => {
  let requests = 0;
  const app = wasmShell(async () => {
    ++requests;
    return new Response(emptyWasm, { headers: { "Content-Type": "application/octet-stream" } });
  });
  await assert.rejects(WebAssembly.instantiateStreaming(app.window.fetch(app.wasmURL), {}));
  const response = await app.window.fetch(app.wasmURL, { credentials: "same-origin" });
  const result = await WebAssembly.instantiate(await response.arrayBuffer(), {});
  assert.equal(requests, 2);
  assert.ok(result.instance instanceof WebAssembly.Instance);
  assertHidden(app);
});
