use gtk::prelude::*;
use gtk4 as gtk;

pub(super) fn build_stepper(workflow_stack: &gtk::Stack) -> (gtk::Box, Vec<gtk::Button>) {
    let stepper = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    stepper.add_css_class("top-stepper");
    stepper.set_halign(gtk::Align::Center);
    stepper.set_hexpand(true);

    let mut step_buttons = Vec::new();
    let steps = [
        ("files", "Select Files"),
        ("preflight", "Preflight"),
        ("review", "Output Plan"),
        ("hide", "Confirm"),
    ];

    for (index, (page_name, label_text)) in steps.iter().enumerate() {
        let step_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
        step_box.add_css_class("stepper-item");

        let number_circle = gtk::Label::new(Some(&(index + 1).to_string()));
        number_circle.add_css_class("stepper-number");

        let label = gtk::Label::new(Some(label_text));
        label.add_css_class("stepper-label");

        step_box.append(&number_circle);
        let label_row = gtk::Box::new(gtk::Orientation::Horizontal, 5);
        label_row.add_css_class("stepper-label-row");
        let checkmark = gtk::Label::new(Some("✓"));
        checkmark.add_css_class("stepper-checkmark");
        label_row.append(&label);
        label_row.append(&checkmark);
        step_box.append(&label_row);

        let button = gtk::Button::new();
        button.set_child(Some(&step_box));
        button.add_css_class("stepper-button-v2");
        button.set_has_frame(false);
        button.set_tooltip_text(Some(label_text));
        if index == 0 {
            button.add_css_class("stepper-active");
        }

        let page_name = (*page_name).to_string();
        let workflow_stack_clone = workflow_stack.clone();
        button.connect_clicked(move |_| {
            workflow_stack_clone.set_visible_child_name(&page_name);
        });

        stepper.append(&button);
        step_buttons.push(button);

        // Add connecting line if not last
        if index < steps.len() - 1 {
            let line = gtk::Box::new(gtk::Orientation::Horizontal, 0);
            line.add_css_class("stepper-line");
            line.set_size_request(46, 2);
            line.set_valign(gtk::Align::Start);
            stepper.append(&line);
        }
    }

    connect_stepper_state_v2(workflow_stack, &step_buttons);
    (stepper, step_buttons)
}

fn connect_stepper_state_v2(workflow_stack: &gtk::Stack, step_buttons: &[gtk::Button]) {
    let step_buttons = step_buttons.to_vec();
    workflow_stack.connect_visible_child_name_notify(move |stack| {
        let page_name = stack.visible_child_name().unwrap_or_else(|| "files".into());
        let active_index = match page_name.as_str() {
            "preflight" => 1,
            "review" => 2,
            "hide" => 3,
            _ => 0,
        };

        for (i, button) in step_buttons.iter().enumerate() {
            button.remove_css_class("stepper-active");
            button.remove_css_class("stepper-completed");

            if i == active_index {
                button.add_css_class("stepper-active");
            } else if i < active_index {
                button.add_css_class("stepper-completed");
            }
        }
    });
}

pub(super) fn go_to_step_button(
    label_text: &str,
    page_name: &'static str,
    workflow_stack: &gtk::Stack,
) -> gtk::Button {
    let button = gtk::Button::with_label(label_text);
    button.add_css_class("action");
    let workflow_stack_clone = workflow_stack.clone();
    button.connect_clicked(move |_| {
        workflow_stack_clone.set_visible_child_name(page_name);
    });
    button
}
