mod app;
mod command_builder;
mod path_utils;
mod theme;
mod validation;

use gtk::prelude::*;
use gtk4 as gtk;

// Application entrypoint for GTK frontend
fn main() {
    // Application id is used by GTK for session-level identity
    let application = gtk::Application::builder()
        .application_id("com.inplainsight.studio")
        .build();

    application.connect_startup(|_| {
        // Global theme is installed once when app starts
        theme::install_theme();
    });

    application.connect_activate(|application| {
        // Build all widgets after GTK reports app activation
        app::build_ui(application);
    });

    // Run enters the GTK event loop and returns on app exit
    application.run();
}
