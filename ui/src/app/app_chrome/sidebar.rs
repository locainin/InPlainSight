use gtk::prelude::*;
use gtk4 as gtk;

// Sidebar widget bundle returned to the app shell
pub(in crate::app) struct SidebarPanel {
    pub(in crate::app) container: gtk::Box,
}

// Build the left navigation rail and bottom utility actions
pub(in crate::app) fn build_sidebar(
    hide_button: &gtk::Button,
    extract_button: &gtk::Button,
    theme_button: &gtk::Button,
) -> SidebarPanel {
    // Fixed width keeps the workflow and inspector from shifting between modes
    let sidebar = gtk::Box::new(gtk::Orientation::Vertical, 18);
    sidebar.add_css_class("sidebar");
    sidebar.set_width_request(188);
    sidebar.set_size_request(188, -1);
    sidebar.set_hexpand(false);

    // Navigation controls are passed in so app_window owns their signal wiring
    let brand_row = build_brand_row();
    let nav = gtk::Box::new(gtk::Orientation::Vertical, 10);
    nav.add_css_class("sidebar-nav");
    nav.append(hide_button);
    nav.append(extract_button);

    let about_button = build_tool_button("ⓘ", "About");

    // Theme and about live at the bottom where persistent utility actions belong
    let bottom = gtk::Box::new(gtk::Orientation::Vertical, 12);
    bottom.add_css_class("sidebar-bottom");
    bottom.set_valign(gtk::Align::End);
    bottom.set_vexpand(true);

    bottom.append(theme_button);
    bottom.append(&about_button);

    sidebar.append(&brand_row);
    sidebar.append(&nav);
    sidebar.append(&bottom);

    SidebarPanel { container: sidebar }
}

pub(in crate::app) fn build_sidebar_button(label_text: &str, active: bool) -> gtk::Button {
    // Icons are fixed per mode so the navigation remains easy to scan
    let icon_text = match label_text {
        "Hide" => "🔒",
        "Extract" => "📂",
        _ => "•",
    };

    // Use a child box instead of button text so icon and label spacing is stable
    let box_widget = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    let icon = gtk::Label::new(Some(icon_text));
    icon.add_css_class("sidebar-icon");
    let label = gtk::Label::new(Some(label_text));

    box_widget.append(&icon);
    box_widget.append(&label);

    let button = gtk::Button::new();
    button.set_child(Some(&box_widget));
    button.add_css_class("sidebar-button");
    button.set_tooltip_text(Some(label_text));
    if active {
        button.add_css_class("sidebar-button-active");
    }
    button
}

fn build_tool_button(icon_text: &str, label_text: &str) -> gtk::Button {
    // Tool buttons share nav styling but use lower visual priority
    let box_widget = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    let icon = gtk::Label::new(Some(icon_text));
    icon.add_css_class("sidebar-icon");
    let label = gtk::Label::new(Some(label_text));

    box_widget.append(&icon);
    box_widget.append(&label);

    let button = gtk::Button::new();
    button.set_child(Some(&box_widget));
    button.add_css_class("sidebar-button");
    button.add_css_class("sidebar-tool-link");
    button.set_tooltip_text(Some(label_text));
    button
}

fn build_brand_row() -> gtk::Box {
    // The brand block is compact so the sidebar stays useful on short windows
    let brand_row = gtk::Box::new(gtk::Orientation::Vertical, 8);
    brand_row.add_css_class("brand-row");

    let shield = gtk::Label::new(Some("🛡"));
    shield.add_css_class("brand-mark");
    shield.set_halign(gtk::Align::Start);

    let brand_copy = gtk::Box::new(gtk::Orientation::Vertical, 4);
    let brand = gtk::Label::new(Some("InPlainSight"));
    brand.add_css_class("brand-title");
    brand.set_xalign(0.0);
    let subtitle = gtk::Label::new(Some("Steganography for\nlossless images"));
    subtitle.add_css_class("brand-subtitle");
    subtitle.set_xalign(0.0);
    brand_copy.append(&brand);
    brand_copy.append(&subtitle);

    let top_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    top_box.append(&shield);
    top_box.append(&brand_copy);

    brand_row.append(&top_box);
    brand_row
}
