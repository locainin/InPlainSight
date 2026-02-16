// Panel module exports composed hide and extract cards used by app layout
mod extract_panel;
mod hide_panel;
mod passphrase_widgets;
mod payload_widgets;
mod step_widgets;

// Card builders return fully wired GTK containers for each workflow section
pub use extract_panel::{assemble_extract_card, build_extract_panel};
pub use hide_panel::{assemble_hide_card, build_hide_panel};

// Shared widget builders keep hide panel sections consistent
pub(crate) use passphrase_widgets::{build_hide_passphrase_stack, build_passphrase_text_entry};
pub(crate) use payload_widgets::{build_payload_stack, build_payload_text_view};
pub(crate) use step_widgets::build_feature_badge_row;
pub(crate) use step_widgets::{build_step_heading, build_step_revealer};
