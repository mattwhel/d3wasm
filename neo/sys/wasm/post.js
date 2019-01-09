(function(d, script) {
  script = d.createElement('script');
  script.type = 'text/javascript';
  script.async = false;
  script.onload = function(){
    // remote script has loaded
  };
  script.src = 'demo_chunk00.js';
  d.getElementsByTagName('head')[0].appendChild(script);
}(document));

(function(d, script) {
  script = d.createElement('script');
  script.type = 'text/javascript';
  script.async = false;
  script.onload = function(){
    // remote script has loaded
  };
  script.src = 'demo_chunk01.js';
  d.getElementsByTagName('head')[0].appendChild(script);
}(document));

//python $EMSCRIPTEN/tools/file_packager.py demo_chunk00.data --preload data/demo_chunk00.pk4@/usr/local/share/dhewm3/base/ --js-output=demo_chunk00.js --use-preload-cache --no-heap-copy
//python $EMSCRIPTEN/tools/file_packager.py demo_chunk01.data --preload data/demo_chunk01.pk4@/usr/local/share/dhewm3/base/ --js-output=demo_chunk01.js --use-preload-cache --no-heap-copy
