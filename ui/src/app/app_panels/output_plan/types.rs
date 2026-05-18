use gtk4 as gtk;

// All widgets needed after preflight are held together so wiring does not search the tree
#[derive(Clone)]
pub struct OutputPlanView {
    // Root section for step three
    pub section: gtk::Box,
    // Main action changes text after preflight knows the real image count
    pub cta_button: gtk::Button,
    // Stack switches between split and single layouts
    pub mode_stack: gtk::Stack,
    pub split: SplitOutputPlanWidgets,
    pub single: SingleOutputPlanWidgets,
}

// Labels that preflight updates for the multi-image path
#[derive(Clone)]
pub struct SplitOutputPlanWidgets {
    pub result_label: gtk::Label,
    pub payload_size_label: gtk::Label,
    pub capacity_label: gtk::Label,
    pub images_required_label: gtk::Label,
    pub notice_title_label: gtk::Label,
    pub file_count_label: gtk::Label,
    pub file_name_labels: Vec<gtk::Label>,
    pub file_size_labels: Vec<gtk::Label>,
    pub file_rows: Vec<gtk::Box>,
}

// Labels that preflight updates for the one-image path
#[derive(Clone)]
pub struct SingleOutputPlanWidgets {
    pub result: gtk::Label,
    pub payload_size: gtk::Label,
    pub capacity: gtk::Label,
    pub images_required: gtk::Label,
    pub output_size: gtk::Label,
}
