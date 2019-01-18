(function(d, script) {
  script = d.createElement('script');
  script.type = 'text/javascript';
  script.async = false;
  script.onload = function(){
    // remote script has loaded
  };
  //script.src = 'demo_game00.js';
  d.getElementsByTagName('head')[0].appendChild(script);
}(document));

(function(d, script) {
  script = d.createElement('script');
  script.type = 'text/javascript';
  script.async = true;
  script.onload = function(){
    // remote script has loaded
  };
  //script.src = 'demo_game01.js';
  d.getElementsByTagName('head')[0].appendChild(script);
}(document));

// For chunked demo data
//python $EMSCRIPTEN/tools/file_packager.py demo_game00.data --preload data/chunked/demo_game00.pk4@/usr/local/share/dhewm3/base/ --js-output=demo_game00.js --use-preload-cache --no-heap-copy
//python $EMSCRIPTEN/tools/file_packager.py demo_game01.data --preload data/chunked/demo_game01.pk4@/usr/local/share/dhewm3/base/ --js-output=demo_game01.js --use-preload-cache --no-heap-copy
//be sure to update the result.js file to enable the loading in 'postRun' (and not 'preRun' like it is done by default)