// The side summary reports current state only
// Capacity and split decisions must come from CLI preflight, not local guessing
mod formatting;
mod live;
mod types;
mod view;

// The app window only needs the assembled summary panel
pub use view::build_plan_summary_panel;
