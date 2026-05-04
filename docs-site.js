(function () {
  function ready(fn) {
    if (document.readyState === "loading") {
      document.addEventListener("DOMContentLoaded", fn);
    } else {
      fn();
    }
  }

  function moveThemeToggle() {
    var slot = document.getElementById("theme-toggle-slot");
    if (!slot) return;

    var toggle = document.querySelector("doxygen-awesome-dark-mode-toggle");
    if (!toggle) return;

    toggle.title = "Toggle light and dark mode";
    toggle.setAttribute("aria-label", "Toggle light and dark mode");
    slot.appendChild(toggle);
  }

  function setupSidebarToggle() {
    var button = document.getElementById("sidebar-toggle");
    if (!button) return;

    var key = "vr-docs-sidebar-collapsed";
    var apply = function (collapsed) {
      document.documentElement.classList.toggle("sidebar-collapsed", collapsed);
      button.setAttribute("aria-pressed", collapsed ? "true" : "false");
      button.title = collapsed ? "Show navigation panel" : "Hide navigation panel";
    };

    apply(localStorage.getItem(key) === "true");

    button.addEventListener("click", function () {
      var collapsed = !document.documentElement.classList.contains("sidebar-collapsed");
      localStorage.setItem(key, collapsed ? "true" : "false");
      apply(collapsed);
      window.dispatchEvent(new Event("resize"));
    });
  }

  function tidyNavControls() {
    var navSync = document.getElementById("nav-sync");
    if (navSync) {
      navSync.setAttribute("title", "Keep the navigation tree synced with the current page");
      navSync.setAttribute("aria-label", "Sync navigation tree");
      navSync.setAttribute("role", "button");
    }

    var splitbar = document.getElementById("splitbar");
    if (splitbar) {
      splitbar.setAttribute("title", "Navigation divider");
      splitbar.setAttribute("aria-hidden", "true");
    }
  }

  function setupProgressBar() {
    var bar = document.createElement("div");
    bar.id = "scroll-progress";
    document.body.appendChild(bar);

    var update = function () {
      var doc = document.documentElement;
      var max = Math.max(1, doc.scrollHeight - window.innerHeight);
      var progress = Math.min(1, Math.max(0, window.scrollY / max));
      bar.style.transform = "scaleX(" + progress + ")";
    };

    update();
    window.addEventListener("scroll", update, { passive: true });
    window.addEventListener("resize", update);
  }

  function setupBackToTop() {
    var button = document.createElement("button");
    button.id = "back-to-top";
    button.type = "button";
    button.textContent = "Top";
    button.setAttribute("aria-label", "Back to top");
    document.body.appendChild(button);

    var update = function () {
      button.classList.toggle("visible", window.scrollY > 360);
    };

    button.addEventListener("click", function () {
      window.scrollTo({ top: 0, behavior: "smooth" });
    });

    update();
    window.addEventListener("scroll", update, { passive: true });
  }

  function setupCopyButtons() {
    document.querySelectorAll("[data-copy]").forEach(function (button) {
      button.addEventListener("click", function () {
        var text = button.getAttribute("data-copy");
        if (!text) return;

        navigator.clipboard.writeText(text).then(function () {
          var original = button.textContent;
          button.textContent = "Copied";
          setTimeout(function () {
            button.textContent = original;
          }, 1400);
        });
      });
    });
  }

  function addSearchHint() {
    var box = document.getElementById("MSearchBox");
    if (!box || document.getElementById("search-hint")) return;

    var hint = document.createElement("div");
    hint.id = "search-hint";
    hint.textContent = "Search classes, files and functions";
    box.insertAdjacentElement("afterend", hint);
  }

  function enhanceFooter() {
    var footer = document.querySelector("#nav-path li.footer");
    if (!footer || document.getElementById("vr-footer-links")) return;

    var links = document.createElement("span");
    links.id = "vr-footer-links";
    links.innerHTML = '<a href="https://github.com/2025-GROUP-1/2025_GROUP_1">Repository</a><span>Docs v1.1.0</span><span>EEEE2076 Group 1</span>';
    footer.appendChild(links);
  }

  ready(function () {
    moveThemeToggle();
    setupSidebarToggle();
    tidyNavControls();
    setupProgressBar();
    setupBackToTop();
    setupCopyButtons();
    addSearchHint();
    enhanceFooter();

    setTimeout(moveThemeToggle, 250);
    setTimeout(tidyNavControls, 250);
    setTimeout(addSearchHint, 250);
    setTimeout(enhanceFooter, 250);

    window.addEventListener("resize", function () {
      setTimeout(moveThemeToggle, 0);
      setTimeout(tidyNavControls, 0);
    });
  });
})();
