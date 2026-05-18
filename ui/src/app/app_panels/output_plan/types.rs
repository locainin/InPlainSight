use gtk4 as gtk;

#[derive(Clone)]
pub struct OutputPlanView {
    pub section: gtk::Box,
    pub cta_button: gtk::Button,
    pub mode_stack: gtk::Stack,
    pub split: SplitOutputPlanWidgets,
    pub single: SingleOutputPlanWidgets,
}

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

#[derive(Clone)]
pub struct SingleOutputPlanWidgets {
    pub result: gtk::Label,
    pub payload_size: gtk::Label,
    pub capacity: gtk::Label,
    pub images_required: gtk::Label,
    pub output_size: gtk::Label,
}
