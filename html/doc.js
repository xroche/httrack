/* Shared behaviour for the documentation pages: click an image to enlarge it, and
   mark the section being read in the sidebar. Both are optional, and both are
   written to fail quietly on an old browser rather than break the page: a Windows 7
   machine may still open these files in Internet Explorer. */

(function () {
	"use strict";

	function each(list, fn) {
		Array.prototype.forEach.call(list, fn);
	}

	/* ---- click to enlarge ---- */

	function zoomable(node) {
		while (node && node !== document.body) {
			if (node.tagName === "IMG" && node.parentNode &&
				node.parentNode.tagName === "FIGURE") {
				return node;
			}
			node = node.parentNode;
		}
		return null;
	}

	function enlarge() {
		var zoom = document.getElementById("zoom");
		if (!zoom || typeof zoom.showModal !== "function") {
			return;
		}
		var target = zoom.getElementsByTagName("img")[0];
		document.addEventListener("click", function (event) {
			var img = zoomable(event.target);
			if (!img) {
				return;
			}
			event.preventDefault();
			target.src = img.src;
			target.alt = img.alt;
			zoom.showModal();
		});
		zoom.addEventListener("click", function () {
			zoom.close();
		});
	}

	/* ---- highlight the section being read ---- */

	function highlight() {
		var marks = [];
		each(document.querySelectorAll(".toc a, .tabstrip a"), function (a) {
			if (a.hash && a.hash.length > 1) {
				marks.push(a);
			}
		});
		if (!marks.length || !window.IntersectionObserver) {
			return;
		}

		var byId = {};
		each(marks, function (a) {
			var id = a.hash.slice(1);
			(byId[id] = byId[id] || []).push(a);
		});

		var visible = {};
		var observer = new IntersectionObserver(function (entries) {
			each(entries, function (entry) {
				visible[entry.target.id] = entry.isIntersecting;
			});
			var top = Object.keys(byId).filter(function (id) {
				return visible[id];
			})[0];
			each(marks, function (a) {
				if (a.getAttribute("aria-current") === "true") {
					a.removeAttribute("aria-current");
				}
			});
			if (top) {
				each(byId[top], function (a) {
					a.setAttribute("aria-current", "true");
				});
			}
		}, { rootMargin: "-10% 0px -70% 0px" });

		Object.keys(byId).forEach(function (id) {
			var el = document.getElementById(id);
			if (el) {
				observer.observe(el);
			}
		});
	}

	function start() {
		enlarge();
		highlight();
	}

	if (document.readyState === "loading") {
		document.addEventListener("DOMContentLoaded", start);
	} else {
		start();
	}
})();
