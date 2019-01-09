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
