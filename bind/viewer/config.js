/*
 * Deployment settings for the binding reference.
 *
 * The page is a static file: copy index.html and this file (plus the optional
 * api_embed.js snapshot) to any web server and it pulls the IR straight from
 * GitHub.  Edit irUrl when the repository or branch changes; leave it empty to
 * keep the viewer local-only.
 *
 *   irUrl          raw JSON URL of bind/api_reference.json
 *   refreshSeconds re-fetch every N seconds (0 disables the timer)
 *
 * A `?ir=<url>` query parameter on the page always wins, which is handy for
 * previewing a branch before merging it.
 */
window.URGE_VIEWER_CONFIG = {
  irUrl: "https://raw.githubusercontent.com/Admenri/urge/master/bind/api_reference.json",
  refreshSeconds: 0,
};
