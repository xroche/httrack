/* Progressive enhancement for guide.html: the platform switcher and the option
   filter. Image zoom and sidebar highlighting are shared, in doc.js. Everything
   here is optional: with scripting off the page still carries every platform's
   text and screenshots. */

(function () {
	"use strict";

	var PLATFORMS = ["win", "web", "droid"];
	var STORE = "httrack-guide-platform";

	/* Chrome denies storage to file:// origins, and the doc is read from disk. */
	function remembered() {
		try {
			return localStorage.getItem(STORE);
		} catch (e) {
			return null;
		}
	}

	function remember(platform) {
		try {
			localStorage.setItem(STORE, platform);
		} catch (e) {
			/* not fatal: the hash still carries the choice */
		}
	}

	/* "#win/opt-limits" -> {platform: "win", target: "opt-limits"}, and a bare
	   "#win" is a platform, not a target of that name. */
	function parseHash() {
		var hash = location.hash.replace(/^#/, "");
		var slash = hash.indexOf("/");
		if (slash < 0) {
			return PLATFORMS.indexOf(hash) !== -1
				? { platform: hash, target: "" }
				: { platform: null, target: hash };
		}
		if (PLATFORMS.indexOf(hash.slice(0, slash)) !== -1) {
			return { platform: hash.slice(0, slash), target: hash.slice(slash + 1) };
		}
		return { platform: null, target: hash };
	}

	function setPlatform(platform) {
		document.documentElement.setAttribute("data-platform", platform);
	}

	/* Runs before first paint, so the page never flashes the other platforms. */
	var initial = parseHash().platform || remembered() ||
		(/Android/i.test(navigator.userAgent) ? "droid" :
			/Windows/i.test(navigator.userAgent) ? "win" : "web");
	setPlatform(initial);

	document.addEventListener("DOMContentLoaded", function () {
		var current = document.documentElement.getAttribute("data-platform");

		/* ---- platform switcher ---- */

		var buttons = [].slice.call(document.querySelectorAll(".platforms button"));

		function select(platform, push) {
			current = platform;
			setPlatform(platform);
			remember(platform);
			buttons.forEach(function (b) {
				b.setAttribute("aria-pressed", String(b.dataset.platform === platform));
			});
			if (push) {
				var target = parseHash().target;
				history.replaceState(null, "", "#" + platform + (target ? "/" + target : ""));
			}
		}

		buttons.forEach(function (b) {
			b.addEventListener("click", function () {
				select(b.dataset.platform, true);
			});
		});
		select(current, false);

		/* A "#win/opt-limits" link has no element of that id: scroll it ourselves. */
		function jumpToHash() {
			var parsed = parseHash();
			if (parsed.platform) {
				select(parsed.platform, false);
			}
			if (parsed.target) {
				var el = document.getElementById(parsed.target);
				if (el) {
					el.scrollIntoView();
				}
			}
		}

		window.addEventListener("hashchange", jumpToHash);
		if (parseHash().platform) {
			jumpToHash();
		}

		/* ---- option filter ---- */

		var filter = document.getElementById("optfilter");
		var count = document.getElementById("optcount");
		if (filter) {
			var options = [].slice.call(document.querySelectorAll(".opt"));
			options.forEach(function (opt) {
				opt.dataset.haystack = opt.textContent.toLowerCase();
			});
			filter.addEventListener("input", function () {
				var needle = filter.value.trim().toLowerCase();
				var shown = 0;
				options.forEach(function (opt) {
					var hit = !needle || opt.dataset.haystack.indexOf(needle) !== -1;
					opt.hidden = !hit;
					shown += hit ? 1 : 0;
				});
				/* Fold away a tab whose options all filtered out. */
				[].slice.call(document.querySelectorAll("section.tab")).forEach(function (section) {
					section.hidden = needle !== "" &&
						!section.querySelector(".opt:not([hidden])");
				});
				count.textContent = needle ? shown + " of " + options.length : "";
			});
		}
	});
})();
