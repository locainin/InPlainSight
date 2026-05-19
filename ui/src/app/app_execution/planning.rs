use serde::Deserialize;
use std::fmt;

use crate::command_builder::CommandExecution;

#[derive(Debug, Deserialize)]
// "cover" section from `inplainsight info --json`
// This is used for user-facing suggestions when a payload does not fit
struct InfoCover {
    format: String,
    width: u32,
    height: u32,
    channels: u8,
    decoded_bytes: u64,
}

#[derive(Debug, Deserialize)]
// "caps" section from `inplainsight info --json`
// This is split out so serde mapping stays simple and obvious
struct InfoCaps {
    max_payload_per_shard: u64,
}

#[derive(Debug, Deserialize)]
// "payload" section from `inplainsight info --json`
// These fields drive single-cover vs split planning behavior in the UI
struct InfoPayload {
    provided: bool,
    payload_bytes: u64,
    fits_single: bool,
    required_shards: u64,
    limiting_factor: String,
}

#[derive(Debug, Deserialize)]
// "computed" section from `inplainsight info --json`
// This holds the pure cover-derived payload ceiling
struct InfoComputed {
    max_payload_by_cover_bytes: u64,
}

#[derive(Debug, Deserialize)]
// "plan" section from `inplainsight info --json`
// This summarizes the CLI's recommended execution mode
struct InfoPlan {
    output_cap_risk: bool,
}

#[derive(Debug, Deserialize)]
// Top-level envelope emitted by the info command
// This keeps parsing strict so schema drifts fail fast
struct InfoEnvelope {
    plan_schema_version: u32,
    cover: InfoCover,
    plan: InfoPlan,
    caps: InfoCaps,
    computed: InfoComputed,
    payload: InfoPayload,
}

#[derive(Debug, Clone)]
// Parsed planner output used by hide execution
pub struct HidePreflightPlan {
    pub(crate) payload_provided: bool,
    pub(crate) payload_bytes: u64,
    pub(crate) fits_single: bool,
    pub(crate) required_shards: u64,
    pub(crate) max_payload_by_cover_bytes: u64,
    pub(crate) max_payload_per_shard: u64,
    pub(crate) limiting_factor: String,
    pub(crate) cover_format: String,
    pub(crate) cover_width: u32,
    pub(crate) cover_height: u32,
    pub(crate) cover_channels: u8,
    pub(crate) cover_decoded_bytes: u64,
    pub(crate) plan_output_cap_risk: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HidePreflightPlanError {
    JsonParse(String),
    UnsupportedSchemaVersion(u32),
}

impl fmt::Display for HidePreflightPlanError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::JsonParse(error_text) => {
                write!(formatter, "failed to parse info JSON output: {error_text}")
            }
            Self::UnsupportedSchemaVersion(schema_version) => {
                write!(
                    formatter,
                    "unsupported info plan schema version: {schema_version}"
                )
            }
        }
    }
}

pub fn parse_hide_preflight_json(
    command_execution: &CommandExecution,
) -> Result<HidePreflightPlan, HidePreflightPlanError> {
    // Parse raw stdout directly so non-JSON text is treated as a hard failure
    // The UI treats this as a protocol boundary and fails closed on schema drift
    let parsed_envelope: InfoEnvelope = serde_json::from_str(command_execution.stdout_text.trim())
        .map_err(|error_value| HidePreflightPlanError::JsonParse(error_value.to_string()))?;

    // Planner schema version must match what this UI understands
    if parsed_envelope.plan_schema_version != 1u32 {
        return Err(HidePreflightPlanError::UnsupportedSchemaVersion(
            parsed_envelope.plan_schema_version,
        ));
    }

    // Copy validated planner fields into the execution-facing shape
    Ok(HidePreflightPlan {
        payload_provided: parsed_envelope.payload.provided,
        payload_bytes: parsed_envelope.payload.payload_bytes,
        fits_single: parsed_envelope.payload.fits_single,
        required_shards: parsed_envelope.payload.required_shards,
        max_payload_by_cover_bytes: parsed_envelope.computed.max_payload_by_cover_bytes,
        max_payload_per_shard: parsed_envelope.caps.max_payload_per_shard,
        limiting_factor: parsed_envelope.payload.limiting_factor,
        cover_format: parsed_envelope.cover.format,
        cover_width: parsed_envelope.cover.width,
        cover_height: parsed_envelope.cover.height,
        cover_channels: parsed_envelope.cover.channels,
        cover_decoded_bytes: parsed_envelope.cover.decoded_bytes,
        plan_output_cap_risk: parsed_envelope.plan.output_cap_risk,
    })
}
