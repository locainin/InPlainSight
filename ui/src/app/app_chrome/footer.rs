use gtk::prelude::*;
use gtk4 as gtk;

// Build the persistent footer with security posture and version text
pub(in crate::app) fn build_footer() -> gtk::Box {
    // Footer content is static so it can be created without extra state handles
    let footer = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    footer.add_css_class("footer");

    let security = gtk::Label::new(Some(
        "Security: XChaCha20-Poly1305  *  Argon2id KDF  *  Authenticated extraction",
    ));
    security.add_css_class("footer-text");
    security.set_hexpand(true);
    security.set_xalign(0.5);

    let version = gtk::Label::new(Some("InPlainSight v0.1.0"));
    version.add_css_class("footer-version");
    version.set_xalign(1.0);

    footer.append(&security);
    footer.append(&version);
    footer
}
