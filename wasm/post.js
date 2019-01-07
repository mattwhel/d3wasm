(function(d, script) {
  script = d.createElement('script');
  script.type = 'text/javascript';
  script.async = false;
  script.onload = function(){
    // remote script has loaded
  };
  script.src = 'demo_base.js';
  d.getElementsByTagName('head')[0].appendChild(script);
}(document));
