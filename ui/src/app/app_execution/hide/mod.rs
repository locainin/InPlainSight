// Hide operation orchestration
//
// This module intentionally splits the hide workflow into small files
// The old hide_flow.rs grew too large and made changes risky

// detail.rs holds long user-facing string building
mod detail;
// preflight.rs runs the info --json planner and selects single vs split
mod preflight;
// resolve.rs validates and converts UI inputs into CLI-ready values
mod resolve;
// split.rs handles the split confirmation and split-specific execution path
mod split;
// types.rs holds shared structs and small UI helper functions
mod types;
// wire.rs connects UI signals to the hide workflow
mod wire;

// Export is intentionally small so the rest of the app has one entrypoint
pub(crate) use wire::wire_hide_execution;
