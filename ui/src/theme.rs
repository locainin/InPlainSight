use gtk::gdk;
use gtk4 as gtk;

// Centralized CSS theme for the GTK app
const APP_CSS: &str = include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/assets/app.css"));

// Install style provider once at startup
pub fn install_theme() {
    let css_provider = gtk::CssProvider::new();
    // CSS is embedded at compile time for predictable app startup behavior
    // gtk4-rs exposes this as a fallible API in some versions and as an infallible API in others
    // When the API is infallible, GTK will emit warnings for invalid CSS during runtime
    css_provider.load_from_data(APP_CSS);

    if let Some(display_value) = gdk::Display::default() {
        // Provider priority ensures app CSS overrides generic GTK defaults
        gtk::style_context_add_provider_for_display(
            &display_value,
            &css_provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );
    }
}
