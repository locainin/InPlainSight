use gtk4 as gtk;

// Returned handles for the side panel pieces the app window must wire
pub struct PlanSummaryPanel {
    // Complete panel shown to the right of the workflow
    pub container: gtk::Box,
    // Log clear action lives with the log but is wired by the app shell
    pub clear_log_button: gtk::Button,
}

// Warning labels are updated together so stale status text cannot linger
#[derive(Clone)]
pub struct WarningLabels {
    pub state_pill: gtk::Label,
    pub title: gtk::Label,
    pub detail: gtk::Label,
}

// Detail labels mirror selected inputs and execution choices
#[derive(Clone)]
pub struct DetailLabels {
    pub cover: gtk::Label,
    pub payload: gtk::Label,
    pub plan: gtk::Label,
    pub output: gtk::Label,
    pub pattern: gtk::Label,
    pub method: gtk::Label,
}

// Top metrics stay pending until preflight gives trusted values
#[derive(Clone)]
pub struct MetricLabels {
    pub payload: gtk::Label,
    pub capacity: gtk::Label,
    pub shards: gtk::Label,
}

// Source widgets are read by the summary refresh function
#[derive(Clone)]
pub struct SummarySources {
    pub cover_entry: gtk::Entry,
    pub payload_entry: gtk::Entry,
    pub output_entry: gtk::Entry,
    pub output_dir_entry: gtk::Entry,
    pub pattern_entry: gtk::Entry,
    pub method_dropdown: gtk::DropDown,
}
