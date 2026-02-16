// Execution module wires hide and extract actions to background command runs
mod descriptor;
mod extract_flow;
mod helpers;
mod hide;
mod planning;
mod runner;

// Public wiring functions are consumed by top-level app builder
pub(crate) use extract_flow::wire_extract_execution;
pub(crate) use hide::wire_hide_execution;
