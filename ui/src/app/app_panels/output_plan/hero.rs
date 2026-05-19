use gtk::prelude::*;
use gtk4 as gtk;

// Build the large icon and explanatory copy at the top of step three
pub fn build_output_plan_hero(icon: &gtk::Widget, title_text: &str, detail_text: &str) -> gtk::Box {
    // The hero is intentionally not a card so it matches the concept hierarchy
    let plan_intro = gtk::Box::new(gtk::Orientation::Horizontal, 22);
    plan_intro.add_css_class("output-plan-hero");

    let plan_copy = gtk::Box::new(gtk::Orientation::Vertical, 7);
    plan_copy.set_valign(gtk::Align::Center);
    let plan_title = gtk::Label::new(Some(title_text));
    plan_title.add_css_class("output-plan-title");
    plan_title.set_xalign(0.0);
    plan_title.set_wrap(true);
    let plan_detail = gtk::Label::new(Some(detail_text));
    plan_detail.add_css_class("output-plan-detail");
    plan_detail.set_xalign(0.0);
    plan_detail.set_wrap(true);
    plan_detail.set_wrap_mode(gtk::pango::WrapMode::WordChar);
    plan_copy.append(&plan_title);
    plan_copy.append(&plan_detail);

    plan_intro.append(icon);
    plan_intro.append(&plan_copy);
    plan_intro
}

pub fn build_split_output_plan_icon() -> gtk::Widget {
    // The split asset is bundled so the UI does not depend on runtime file paths
    build_embedded_image(
        include_bytes!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/assets/images/plan-split.png"
        )),
        "output-plan-split-asset",
        96,
    )
}

pub fn build_single_output_plan_icon() -> gtk::Widget {
    // Single-image success uses a separate asset to avoid reusing split language
    build_embedded_image(
        include_bytes!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/assets/images/plan-single-hero.svg"
        )),
        "output-plan-single-asset",
        88,
    )
}

pub fn build_embedded_image(
    image_bytes: &'static [u8],
    class_name: &str,
    pixel_size: i32,
) -> gtk::Widget {
    // Embedded bytes keep release builds self-contained
    let bytes = gtk::glib::Bytes::from_static(image_bytes);
    if let Ok(texture) = gtk::gdk::Texture::from_bytes(&bytes) {
        // GTK Picture preserves raster/SVG scaling without custom drawing code
        let picture = gtk::Picture::for_paintable(&texture);
        picture.add_css_class(class_name);
        picture.set_content_fit(gtk::ContentFit::Contain);
        picture.set_size_request(pixel_size, pixel_size);
        picture.set_can_shrink(false);
        picture.set_hexpand(false);
        picture.set_vexpand(false);
        picture.set_halign(gtk::Align::Start);
        picture.set_valign(gtk::Align::Center);
        return picture.upcast();
    }

    // Fallback is deliberately plain because asset decode failure should be obvious
    let fallback = gtk::Label::new(Some("IMG"));
    fallback.add_css_class(class_name);
    fallback.upcast()
}
