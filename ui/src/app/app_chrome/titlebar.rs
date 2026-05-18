use gtk::prelude::*;
use gtk4 as gtk;

pub(in crate::app) fn build_titlebar(status_label: &gtk::Label) -> gtk::Box {
    let bar = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    bar.add_css_class("titlebar");

    let traffic_lights = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    traffic_lights.add_css_class("traffic-lights");
    for class_name in ["close", "minimize", "zoom"] {
        let light = gtk::Box::new(gtk::Orientation::Horizontal, 0);
        light.add_css_class("traffic-light");
        light.add_css_class(class_name);
        light.set_size_request(12, 12);
        light.set_valign(gtk::Align::Center);
        traffic_lights.append(&light);
    }

    let title = gtk::Label::new(Some("InPlainSight"));
    title.add_css_class("window-title");
    title.set_hexpand(true);
    title.set_xalign(0.5);

    let right_tools = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    right_tools.add_css_class("titlebar-tools");
    status_label.add_css_class("titlebar-status");
    right_tools.append(status_label);

    bar.append(&traffic_lights);
    bar.append(&title);
    bar.append(&right_tools);
    bar
}
