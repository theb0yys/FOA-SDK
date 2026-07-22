# Deterministic suite-wizard view-model and confirmation contract

`Installer/SuiteWizard/ViewModel/` owns the pure presentation model and the review-only confirmation contract that sit between the deterministic resolver and the future wizard UI.

It consumes one exact resolver plan, verifies `plan_sha256`, and derives:

- stable suite, compatibility and selection summaries;
- package rows in resolver order;
- flattened planned-file rows;
- package/file/byte totals;
- licence, notice, lifecycle and elevation information;
- resolver warnings;
- a deterministic set of required acknowledgements;
- `view_model_sha256`.

The future wizard renders this model. It must not parse package manifests, resolve dependencies, change package order, suppress resolver diagnostics, or synthesize a different planned payload.

## Layout

```text
Installer/SuiteWizard/ViewModel/
├── README.md
├── view-model.schema.json
├── confirmation.schema.json
└── Source/
    └── wizard_view_model.py
```

Focused tests live under:

```text
Installer/Tests/SuiteWizardViewModel/
```

## Confirmation flow

1. Build the view-model from the exact resolver plan.
2. Display every package, payload row, warning and required acknowledgement.
3. Collect acknowledgement IDs.
4. Submit the complete expected `plan_sha256`, caller-supplied confirmer identity and caller-supplied UTC timestamp.
5. Create a canonical review-only confirmation.
6. Verify the confirmation against the current plan and current derived view-model before any later handoff.

A confirmation binds:

- exact `plan_sha256`;
- exact `view_model_sha256`;
- the complete sorted acknowledgement set;
- confirmer identity;
- explicit UTC confirmation time;
- a fixed no-authority statement;
- `confirmation_sha256`.

Any plan change, view-model change, acknowledgement change or confirmation mutation invalidates the record.

## Conditional acknowledgements

The model always requires review of:

- the exact plan fingerprint;
- licences and notices;
- preservation of external workspaces.

It additionally requires acknowledgement when the plan contains payload files, permits later network acquisition, requires elevation, contains warnings, requires rollback preparation, or selects a runtime adapter.

## Authority boundary

The module is deliberately pure. It has no process, network, filesystem-write, environment, clock, installer, launcher, deployment, signing or publication dependency.

Confirmation scope is permanently `review-only`. Every authority field remains `false`, including:

- acquisition;
- installation and elevation;
- game launch and runtime execution;
- deployment and save mutation;
- signing and publication;
- catalog mutation and evidence promotion.

A valid confirmation is not permission to execute. A later capability-gated executor must be designed, reviewed and accepted separately, and must reverify the exact plan and confirmation before doing anything.
