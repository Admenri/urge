// (PreRun) Init WebGPU in asynchronization
const initializeWebGPUDevice = () => {
  (async () => {
    const requiredFeatures = [
      "depth-clip-control",
      "depth32float-stencil8",
      "texture-compression-bc",
      "indirect-first-instance",
      "shader-f16",
      "rg11b10ufloat-renderable",
      "bgra8unorm-storage",
      "float32-filterable",
      "unorm16-texture-formats",
      "snorm16-texture-formats",
    ];

    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice({
      requiredFeatures: requiredFeatures.filter(feature => adapter.features.has(feature)),
      requiredLimits: {
        maxColorAttachmentBytesPerSample: adapter.limits.maxColorAttachmentBytesPerSample
      }
    });
    return device;
  })().then(device => {
    Module.preinitializedWebGPUDevice = device;
  }).catch(error => {
    Module.preinitializedWebGPUDevice = null;
    Module.print("Failed to initialize WebGPU.");
  });
};

// (PreRun) Assets loading if need
const resourcePreload = () => {
  for (const key in asyncMappingData) {
    const item = asyncMappingData[key];
    if (!item.preload) continue;

    const fullPath = item.path;
    const pathComponents = fullPath.replace(/\\/g, '/').split('/').filter(part => part && part.trim() !== '');
    const fileName = pathComponents.pop();
    const parentPath = pathComponents.join('/');

    // Create dir if need
    try {
      FS.stat('/' + parentPath);
    } catch (err) {
      var makePath = "";
      pathComponents.forEach((dir) => {
        makePath += '/' + dir;
        try {
          FS.mkdir(makePath);
        } catch (e) {
          // Directory exists, no action
        }
      });
    }

    const loadURL = 'assets/' + fullPath;
    const xhr = new XMLHttpRequest();
    xhr.responseType = "arraybuffer";
    xhr.withCredentials = true;

    const onEnd = () => {
      Module.removeRunDependency(fullPath);
    };

    xhr.onload = function () {
      console.log(`Preloaded: ${fullPath}`);
      FS_createDataFile(parentPath, fileName, new Uint8Array(xhr.response), true, true, false);
      onEnd();
    };

    xhr.onerror = function () {
      console.log(`Error When Preload: ${fullPath}`);
      onEnd();
    };

    Module.addRunDependency(fullPath);
    xhr.open('GET', loadURL);
    xhr.send();
  }
};

// Emscripten module
var Module = {
  preRun: [initializeWebGPUDevice, resourcePreload],
  noImageDecoding: true,
  noAudioDecoding: true,
  totalDependencies: 0,
  print(...args) {
    // These replacements are necessary if you render to raw HTML
    //text = text.replace(/&/g, "&amp;");
    //text = text.replace(/</g, "&lt;");
    //text = text.replace(/>/g, "&gt;");
    //text = text.replace('\n', '<br>', 'g');
    console.log(...args);
  },
  setStatus(text) {
    console.log(text);
  },
};

// Find filename in 'asyncMappingData'
const findFileInAsyncMapping = (filename) => {
  const mapping = asyncMappingData;
  const lowerFilename = filename.toLowerCase();

  const directValue = mapping[lowerFilename];
  if (directValue != undefined)
    return directValue;

  for (const key in mapping) {
    const dotIndex = key.lastIndexOf('.');
    const keyFilename = key.substring(0, dotIndex).toLowerCase();
    if (keyFilename === lowerFilename)
      return mapping[key];
  }

  return null;
};

// Reference: components/filesystem/io_service.cc
window.asyncMappingFileCache = {};
window.isAsyncFileCached = (inFilepath) => {
  // Make emscripten path
  const pathComponents = inFilepath.replace(/\\/g, '/').split('/').filter(part => part && part.trim() !== '');
  const fileName = pathComponents.pop();
  const parentPath = pathComponents.join('/');
  const filePath = (parentPath === '' ? fileName : parentPath + '/' + fileName);

  // Find with async mapping
  const mappingFile = findFileInAsyncMapping(filePath);
  if (mappingFile === null) return false;
  return asyncMappingFileCache.hasOwnProperty(mappingFile.path);
};

// XHR assets fetch
window.fetchLazyAsset = (inFilepath, completeCallback) => {
  // Make emscripten path
  const pathComponents = inFilepath.replace(/\\/g, '/').split('/').filter(part => part && part.trim() !== '');
  const fileName = pathComponents.pop();
  const parentPath = pathComponents.join('/');
  const filepath = (parentPath === '' ? fileName : parentPath + '/' + fileName);

  // Find mapping in preload list
  const mappingFile = findFileInAsyncMapping(filepath);
  if (mappingFile === null) {
    console.log(`Missing File: ${filepath}`);
    completeCallback();
    return;
  } else {
    // Setup cache current file
    window.asyncMappingFileCache[mappingFile.path] = true;

    // Skip if preload
    if (mappingFile.preload) {
      completeCallback();
      return;
    }
  }

  // Create dir if need
  try {
    FS.stat('/' + parentPath);
  } catch (err) {
    var makePath = "";
    pathComponents.forEach((dir) => {
      makePath += '/' + dir;
      try {
        FS.mkdir(makePath);
      } catch (e) {
        // Directory exists, no action
      }
    });
  }

  // Load async asset
  const loadURL = 'assets/' + mappingFile.path;
  const xhr = new XMLHttpRequest();
  xhr.responseType = "arraybuffer";
  xhr.withCredentials = true;

  const onEnd = () => {
    Module.removeRunDependency(loadURL);
    completeCallback();
  };

  xhr.onload = function () {
    console.log(`Downloaded: ${mappingFile.path}`);
    FS_createDataFile(parentPath, fileName, new Uint8Array(xhr.response), true, true, false);
    onEnd();
  };

  xhr.onerror = function () {
    console.error(`Download Error: ${mappingFile.path} - ${xhr.statusText}`);
    onEnd();
  };

  Module.addRunDependency(loadURL);
  xhr.open('GET', loadURL);
  xhr.send();
};
