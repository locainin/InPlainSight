// Chrome modules own the frame around the work area
// Feature panels stay separate so navigation and titlebar rules do not leak into workflow code
mod footer;
mod navigation;
mod sidebar;
mod titlebar;

// Re-export the small builders used by the app window
pub(in crate::app) use footer::build_footer;
pub(in crate::app) use navigation::wire_sidebar_navigation;
pub(in crate::app) use sidebar::{build_sidebar, build_sidebar_button};
pub(in crate::app) use titlebar::build_titlebar;
