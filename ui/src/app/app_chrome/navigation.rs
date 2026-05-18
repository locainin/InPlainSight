use std::rc::Rc;

use gtk::prelude::*;
use gtk4 as gtk;

// Wire the sidebar mode buttons to the main workflow stack
pub(in crate::app) fn wire_sidebar_navigation(
    workflow_stack: &gtk::Stack,
    hide_button: &gtk::Button,
    extract_button: &gtk::Button,
    hide_run_button: &gtk::Button,
    extract_run_button: &gtk::Button,
) {
    // Shared closure keeps Hide and Extract navigation behavior identical
    let set_mode: Rc<dyn Fn(&str)> = Rc::new({
        let workflow_stack = workflow_stack.clone();
        let hide_button = hide_button.clone();
        let extract_button = extract_button.clone();
        let hide_run_button = hide_run_button.clone();
        let extract_run_button = extract_run_button.clone();

        move |mode_name| {
            // The stack name is the source of truth for which product mode is visible
            workflow_stack.set_visible_child_name(mode_name);
            hide_button.remove_css_class("sidebar-button-active");
            extract_button.remove_css_class("sidebar-button-active");

            if mode_name == "extract" {
                // Focus moves to the primary action for keyboard users
                extract_button.add_css_class("sidebar-button-active");
                extract_run_button.grab_focus();
            } else {
                // Hide is the default route for all non-extract mode names
                hide_button.add_css_class("sidebar-button-active");
                hide_run_button.grab_focus();
            }
        }
    });

    {
        // Hide button gets a cloned closure because Extract consumes the original below
        let set_mode_clone = set_mode.clone();
        hide_button.connect_clicked(move |_| {
            set_mode_clone("hide");
        });
    }

    extract_button.connect_clicked(move |_| {
        set_mode("extract");
    });
}
