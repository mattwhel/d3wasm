(function(d, script) {
  script = d.createElement('script');
  script.type = 'text/javascript';
  script.async = false;
  script.onload = function(){
    // remote script has loaded
  };
  script.src = 'demo_bootstrap.js';
  d.getElementsByTagName('head')[0].appendChild(script);
}(document));

// For chunked demo data
//python $EMSCRIPTEN/tools/file_packager.py demo_bootstrap.data --preload data/chunked/demo_bootstrap.pk4@/usr/local/share/dhewm3/base/ --js-output=demo_bootstrap.js --use-preload-cache --no-heap-copy
// + see post.js

// For full demo data
//python $EMSCRIPTEN/tools/file_packager.py demo00.data --preload data/demo/demo00.pk4@/usr/local/share/dhewm3/base/ --js-output=demo00.js --use-preload-cache --no-heap-copy

// For full game data
//python $EMSCRIPTEN/tools/file_packager.py pak.data --preload data/full@/usr/local/share/dhewm3/base/ --js-output=pak.js --use-preload-cache --no-heap-copy

var Module;
if (Module['preRun'] instanceof Array) {
  Module['preRun'].push(setupD3memfs);
} else {
  Module['preRun'] = [setupD3memfs];
}

function setupD3memfs() {
  console.info("Creating dhwem3 data folder");
  FS.createPath('/', 'usr', true, true);
  FS.createPath('/usr', 'local', true, true);
  FS.createPath('/usr/local', 'share', true, true);
  FS.createPath('/usr/local/share', 'dhewm3', true, true);
  FS.createPath('/usr/local/share/dhewm3', 'base', true, true);

  console.info("Mounting user home folder from IDBFS");
  FS.createPath('/', 'home', true, true);
  FS.mount(IDBFS, {}, '/home/web_user');

  FS.syncfs(true, function (err) {
    if (err) {
      console.error(err);
    }
    else {
      console.info("Mounting user home completed");
      FS.createPath('/home/web_user', '.config', true, true);
      FS.createPath('/home/web_user/.config', 'dhewm3', true, true);
      FS.createPath('/home/web_user', '.local', true, true);
      FS.createPath('/home/web_user/.local', 'dhewm3', true, true);
      FS.createPath('/home/web_user/.local/dhewm3', 'base', true, true);
      FS.createPath('/home/web_user/.local/dhewm3/base', 'savegames', true, true);
      Module['removeRunDependency']("setupD3memfs");
    }
  });

  Module['addRunDependency']("setupD3memfs");
}
