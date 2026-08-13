// Tell the server this window is alive, so an abandoned server stops instead of
// outliving the session. The period is the one htsweb.c derives its timeout
// from.
var PING_PERIOD = 5000;

function ping_url(extra) {
  // Unique, or a cached response would never reach the server again.
  return "/ping?t=" + new Date().getTime() + (extra ? "&" + extra : "");
}

// An iframe is the fallback only: reassigning its src can push a history entry,
// which would turn the Back button into a walk through past heartbeats.
function ping_send(url) {
  if (window.fetch) {
    fetch(url, {cache : "no-store"});
    return true;
  }
  var iframe = document.getElementById('pingiframe');
  if (!iframe) {
    return false;
  }
  iframe.src = url;
  return true;
}

function ping_server() {
  if (ping_send(ping_url())) {
    setTimeout(ping_server, PING_PERIOD);
  }
}

// Closing the window is the common case, and waiting out the timeout for it
// would hold the server open long after the user considers it gone.
function ping_leaving() {
  var url = ping_url("e=bye");
  if (navigator.sendBeacon) {
    navigator.sendBeacon(url);
  } else {
    // A request the page's own teardown cannot cancel.
    new Image().src = url;
  }
}

// Old browsers reach none of this and stay on the legacy "wait for the launcher
// to die" mode.
if (document && document.createElement && document.body
    && document.body.appendChild && document.getElementById) {
  if (!window.fetch) {
    var iframe = document.createElement('iframe');
    if (iframe) {
      iframe.id = 'pingiframe';
      iframe.style.display = "none";
      iframe.style.visibility = "hidden";
      iframe.width = iframe.height = 0;
      document.body.appendChild(iframe);
    }
  }
  ping_server();
  // pagehide, not unload: Safari's back/forward cache never fires unload.
  if (window.addEventListener) {
    window.addEventListener('pagehide', ping_leaving, false);
  }
}
