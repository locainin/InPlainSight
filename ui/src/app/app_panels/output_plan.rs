// Step three is made from small modules because split and single-image plans have different UI rules
mod bindings;
mod hero;
mod naming;
mod shared;
mod single;
mod split;
mod types;
mod view;
mod wiring;

// The parent hide panel only needs the built view and the preflight button wiring
pub(super) use types::OutputPlanView;
pub(super) use view::build_output_plan_view;
pub(super) use wiring::wire_review_plan_button;
