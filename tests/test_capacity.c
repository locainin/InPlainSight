// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../include/capacity.h"

// This file tests the capacity math used by the planner
// The goal is to catch off-by-one mistakes and overhead accounting bugs early

static int check_true(int condition, const char *message_text) {
  // Keep the test output simple and consistent
  // fputs is used to avoid any formatting logic in the test runner
  if (!condition) {
    (void)fputs(message_text, stderr);
    (void)fputs("\n", stderr);
    return 0;
  }
  return 1;
}

int main(void) {
  // These structs are filled with specific inputs and then validated against expected outputs
  plainsight_capacity_input capacity_input;
  plainsight_capacity_report capacity_report;
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

  // Large cover case
  // This is a basic sanity check that should always yield a positive payload capacity
  capacity_input.cover_data_bytes = 64ULL * 1024ULL * 1024ULL;
  capacity_input.lsb_bits = 1u;
  capacity_input.density_per_mille = 1000u;
  capacity_input.payload_name_len = 12u;
  capacity_input.mime_len = 24u;

  // The report is deterministic for the same inputs
  result_code = plainsight_capacity_compute_lsb(&capacity_input, &capacity_report);
  if (!check_true(result_code == PLAINSIGHT_OK, "capacity calculation failed")) {
    return 1;
  }

  // With 100 percent density, all cover bytes are usable carrier bytes
  if (!check_true(capacity_report.usable_cover_bytes == capacity_input.cover_data_bytes,
                  "usable cover bytes mismatch")) {
    return 1;
  }

  // For classic LSB mode, one carrier bit is taken from each cover byte
  if (!check_true(capacity_report.usable_carrier_bits == capacity_input.cover_data_bytes,
                  "carrier bits mismatch for 1-bit lsb")) {
    return 1;
  }

  // This only checks that overhead is not larger than the carrier for a large cover
  if (!check_true(capacity_report.max_payload_by_cover_bytes > 0u,
                  "max payload should be positive for large cover")) {
    return 1;
  }

  // Tiny cover case
  // Overhead should dominate and leave no room for payload
  capacity_input.cover_data_bytes = 128u;
  capacity_input.payload_name_len = 64u;
  capacity_input.mime_len = 64u;
  result_code = plainsight_capacity_compute_lsb(&capacity_input, &capacity_report);
  if (!check_true(result_code == PLAINSIGHT_OK, "tiny cover calculation failed")) {
    return 1;
  }
  if (!check_true(capacity_report.max_payload_by_cover_bytes == 0u,
                  "tiny cover should have zero payload capacity after overhead")) {
    return 1;
  }

  // Density scaling case
  // Using 50 percent density should cut usable cover bytes in half
  capacity_input.cover_data_bytes = 4096u;
  capacity_input.density_per_mille = 500u;
  capacity_input.payload_name_len = 8u;
  capacity_input.mime_len = 8u;
  result_code = plainsight_capacity_compute_lsb(&capacity_input, &capacity_report);
  if (!check_true(result_code == PLAINSIGHT_OK, "density calculation failed")) {
    return 1;
  }
  if (!check_true(capacity_report.usable_cover_bytes == 2048u, "density scaling mismatch")) {
    return 1;
  }

  return 0;
}
