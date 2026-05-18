// Execution module wires hide and extract actions to background command runs
mod descriptor;
mod extract_flow;
pub mod helpers;
mod hide;
pub mod planning;
pub mod runner;

// Public wiring functions are consumed by top-level app builder
pub use extract_flow::wire_extract_execution;
pub use hide::wire_hide_execution;
