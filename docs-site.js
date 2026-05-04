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

  ready(function () {
    moveThemeToggle();
    setupSidebarToggle();
    tidyNavControls();

    setTimeout(moveThemeToggle, 250);
    setTimeout(tidyNavControls, 250);

    window.addEventListener("resize", function () {
      setTimeout(moveThemeToggle, 0);
      setTimeout(tidyNavControls, 0);
    });
  });
})();
