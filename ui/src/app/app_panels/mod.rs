// Panel module exports composed hide and extract cards used by app layout
mod extract_panel;
mod formatting;
mod hide_panel;
mod output_plan;
mod passphrase_widgets;
mod payload_widgets;
mod plan_summary;

// Card builders return fully wired GTK containers for each workflow section
pub use extract_panel::{assemble_extract_card, build_extract_panel};
pub use hide_panel::{assemble_hide_card, build_hide_panel};
pub use plan_summary::build_plan_summary_panel;

// Extract panel shares the same passphrase entry styling as hide
pub use passphrase_widgets::build_passphrase_text_entry;
