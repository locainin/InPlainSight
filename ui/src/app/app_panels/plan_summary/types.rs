use gtk4 as gtk;

pub struct PlanSummaryPanel {
    pub container: gtk::Box,
    pub clear_log_button: gtk::Button,
}

#[derive(Clone)]
pub struct WarningLabels {
    pub state_pill: gtk::Label,
    pub title: gtk::Label,
    pub detail: gtk::Label,
}

#[derive(Clone)]
pub struct DetailLabels {
    pub cover: gtk::Label,
    pub payload: gtk::Label,
    pub plan: gtk::Label,
    pub output: gtk::Label,
    pub pattern: gtk::Label,
    pub method: gtk::Label,
}

#[derive(Clone)]
pub struct MetricLabels {
    pub payload: gtk::Label,
    pub capacity: gtk::Label,
    pub shards: gtk::Label,
}

#[derive(Clone)]
pub struct SummarySources {
    pub cover_entry: gtk::Entry,
    pub payload_entry: gtk::Entry,
    pub output_entry: gtk::Entry,
    pub output_dir_entry: gtk::Entry,
    pub pattern_entry: gtk::Entry,
    pub method_dropdown: gtk::DropDown,
}
