mod footer;
mod navigation;
mod sidebar;
mod titlebar;

pub(in crate::app) use footer::build_footer;
pub(in crate::app) use navigation::wire_sidebar_navigation;
pub(in crate::app) use sidebar::{build_sidebar, build_sidebar_button};
pub(in crate::app) use titlebar::build_titlebar;
