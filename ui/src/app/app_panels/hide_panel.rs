// Hide panel is split by page responsibility instead of acting as one large screen file
mod builder;
mod cover;
mod pages;
mod state;
mod stepper;
mod workflow;

// Public entry points stay narrow so callers cannot reach page internals by accident
pub use builder::build_hide_panel;
pub use workflow::assemble_hide_card;
