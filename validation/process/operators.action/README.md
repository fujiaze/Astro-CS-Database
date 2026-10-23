# operators.action M5 validation

Driven by `validation/process/geometry.overlap/m5_contracts.cpp`; raw result in
`observed_m5.jsonl`.

All seven operations are registered and negative-checked (null input ->
`invalid_request`). Positive through the registry with real CSR/diagonal
payloads:
`make_action` (identity + CSR), `apply` (exact [3,3]), `transpose_apply`
(exact [1,5]), `compose`, `action.dependency_blocks` (3 blocks),
`action.serialize_recipe`, `action.row_enclosure` (2 intervals).
Plan example: `apply` R=1088 B, F=65648 B, work=3.
