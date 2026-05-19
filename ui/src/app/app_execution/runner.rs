use std::thread;

use gtk::glib;
use gtk::prelude::*;
use gtk4 as gtk;

use crate::app::app_logging::{LogLevel, append_structured_log, render_command_result};
use crate::app::app_ui_helpers::set_status_fail;
use crate::command_builder::{CommandExecution, run_cli_command};

use std::ffi::OsString;

enum UiMessage {
    // Completed carries either captured command output or launch error text
    Completed(Result<CommandExecution, String>),
}

// Run CLI work outside GTK main thread and forward result back through a channel
pub fn run_command_in_background(
    operation_name: &'static str,
    cli_binary_path: String,
    argument_list: Vec<OsString>,
    descriptor_guards: Vec<std::fs::File>,
    run_button: gtk::Button,
    status_label: gtk::Label,
    log_buffer: gtk::TextBuffer,
) {
    run_task_in_background(
        operation_name,
        run_button,
        status_label,
        log_buffer,
        move || {
            // Keep optional in-memory descriptor handles alive for the full child run
            let _descriptor_guards = descriptor_guards;
            run_cli_command(&cli_binary_path, &argument_list)
        },
    );
}

// Run an arbitrary command task outside GTK main thread and forward result safely
pub fn run_task_in_background<F>(
    operation_name: &'static str,
    run_button: gtk::Button,
    status_label: gtk::Label,
    log_buffer: gtk::TextBuffer,
    task_fn: F,
) where
    F: FnOnce() -> Result<CommandExecution, String> + Send + 'static,
{
    // Standard channel bridges worker thread result back to GTK loop
    let (message_sender, message_receiver) = std::sync::mpsc::channel::<UiMessage>();

    thread::spawn(move || {
        // Worker executes any fallible command orchestration and returns one result payload
        let execution_result = task_fn();
        let _ = message_sender.send(UiMessage::Completed(execution_result));
    });

    poll_background_result(
        operation_name,
        message_receiver,
        run_button,
        status_label,
        log_buffer,
    );
}

// Run a task in a worker thread and invoke a completion callback on the GTK main thread
// This is used when a multi-step UI flow needs to branch on the first command result
pub fn run_task_in_background_with_callback<F, C>(task_fn: F, callback_fn: C)
where
    F: FnOnce() -> Result<CommandExecution, String> + Send + 'static,
    C: FnOnce(Result<CommandExecution, String>) + 'static,
{
    let (message_sender, message_receiver) = std::sync::mpsc::channel::<UiMessage>();

    thread::spawn(move || {
        let execution_result = task_fn();
        let _ = message_sender.send(UiMessage::Completed(execution_result));
    });

    // glib timeout callbacks are `FnMut`, so the callback is stored in an option and consumed once
    let mut callback_fn_option = Some(callback_fn);

    glib::timeout_add_local(
        std::time::Duration::from_millis(40),
        move || match message_receiver.try_recv() {
            Ok(UiMessage::Completed(execution_result)) => {
                // Caller owns re-enable policy because the callback may trigger follow-up commands
                if let Some(callback_value) = callback_fn_option.take() {
                    callback_value(execution_result);
                }
                glib::ControlFlow::Break
            }
            Err(std::sync::mpsc::TryRecvError::Empty) => glib::ControlFlow::Continue,
            Err(std::sync::mpsc::TryRecvError::Disconnected) => {
                // Disconnected channel indicates worker thread ended unexpectedly
                if let Some(callback_value) = callback_fn_option.take() {
                    callback_value(Err("execution channel disconnected".to_string()));
                }
                glib::ControlFlow::Break
            }
        },
    );
}

fn poll_background_result(
    operation_name: &'static str,
    message_receiver: std::sync::mpsc::Receiver<UiMessage>,
    run_button: gtk::Button,
    status_label: gtk::Label,
    log_buffer: gtk::TextBuffer,
) {
    // Polling keeps GTK main thread responsive while worker thread is running
    glib::timeout_add_local(
        std::time::Duration::from_millis(40),
        move || match message_receiver.try_recv() {
            Ok(UiMessage::Completed(execution_result)) => {
                // Re-enable run button after worker completion
                run_button.set_sensitive(true);
                match execution_result {
                    Ok(command_execution) => {
                        render_command_result(
                            operation_name,
                            &status_label,
                            &log_buffer,
                            &command_execution,
                        );
                    }
                    Err(error_text) => {
                        // Launch errors are displayed without assuming exit codes
                        set_status_fail(&status_label, "Execution failed");
                        append_structured_log(
                            &log_buffer,
                            operation_name,
                            LogLevel::Error,
                            &error_text,
                        );
                    }
                }
                glib::ControlFlow::Break
            }
            Err(std::sync::mpsc::TryRecvError::Empty) => glib::ControlFlow::Continue,
            Err(std::sync::mpsc::TryRecvError::Disconnected) => {
                // Disconnected channel indicates worker thread ended unexpectedly
                run_button.set_sensitive(true);
                set_status_fail(&status_label, "Execution channel disconnected");
                append_structured_log(
                    &log_buffer,
                    operation_name,
                    LogLevel::Error,
                    "execution channel disconnected",
                );
                glib::ControlFlow::Break
            }
        },
    );
}
