// Function aimed to ping the webhttrack server regularly to keep it alive
// If the browser window is closed, the server will eventually shutdown
function ping_server() {
	var iframe = document.getElementById('pingiframe');
	if (iframe && iframe.src) {
		iframe.src = iframe.src;
		setTimeout(ping_server, 30000);
	}
}

// Create an invisible iframe to hold the server ping result
// Only modern browsers will support that, but old browsers are compatible
// with the legacy "wait for browser PID" mode
if (document && document.createElement && document.body
    && document.body.appendChild && document.getElementById) {
	var iframe = document.createElement('iframe');
	if (iframe) {
		iframe.id = 'pingiframe';
		iframe.style.display = "none";
		iframe.style.visibility = "hidden";
		iframe.width = iframe.height = 0;
		iframe.src = "/ping";
		document.body.appendChild(iframe);
		ping_server();
	}
}
