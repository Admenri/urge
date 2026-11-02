// Runs after urge-helper.js and before Game.js, only in the macOS host.
(() => {
  "use strict";
  const send = (type, text) => window.webkit.messageHandlers.urge.postMessage({type, text: String(text)});
  const originalInitialized = Module.onRuntimeInitialized;
  let pending = Promise.resolve();
  let saveFailure = false;
  const dirty = new Set();
  const loading = new Set();
  const writable = path => path.startsWith("/") && !/^\/(dev|proc|tmp|home|usr)(\/|$)/.test(path);
  const originalFetch = window.fetchLazyAsset;
  window.fetchLazyAsset = (path, complete) => {
    const real = "/" + window.resolveAsyncPath(path);
    loading.add(real);
    originalFetch(path, () => { loading.delete(real); complete?.(); });
  };

  Module.preRun.push(() => {
    addRunDependency("macos-webgpu-check");
    (async () => {
      if (!window.isSecureContext || !navigator.gpu) throw new Error("WebGPU is unavailable. URGE requires macOS 26.0 or later with WebGPU support.");
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) throw new Error("No WebGPU adapter is available.");
      send("gpu", JSON.stringify({secureContext: isSecureContext, adapter: adapter.info?.description || adapter.info?.architecture || "available"}));
      removeRunDependency("macos-webgpu-check");
    })().catch(error => send("fatal", error.message));
  });

  Module.onRuntimeInitialized = () => {
    const fs = Module.FS;
    fs.trackingDelegate.onWriteToFile = path => { if (writable(path) && !loading.has(path)) dirty.add(path); };
    fs.trackingDelegate.onCloseFile = path => {
      if (!dirty.delete(path)) return;
      const bytes = fs.readFile(path).slice();
      const url = "save/" + path.slice(1).split("/").map(encodeURIComponent).join("/");
      pending = pending.then(async () => {
        const response = await fetch(url, {method: "PUT", body: bytes});
        if (!response.ok) throw new Error(`Could not save ${path}: HTTP ${response.status}`);
        send("saved", path);
      }).catch(error => { saveFailure = true; send("fatal", error.message); });
    };
    originalInitialized?.();
    send("ready", "Ruby/Wasm runtime initialized");
  };
  const originalExit = Module.onExit;
  let exited = false;
  window.urgeHostDidExit = Module.onExit = status => {
    if (exited) return;
    exited = true;
    originalExit?.(status);
    pending.then(() => send("exit", saveFailure ? 1 : status));
  };
  Module.onAbort = reason => send("fatal", reason);
  window.urgeFlushSaves = () => pending.then(() => !saveFailure);
})();
