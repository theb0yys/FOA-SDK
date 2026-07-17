# Privacy and Telemetry

## Current behavior

The TG SDK project does not intentionally add project-specific analytics, advertising, user tracking, or remote telemetry to the editor foundation.

The current workspace, pack, source, evidence, blocker, and future catalog workflows are designed to operate on local files selected or configured by the user.

This statement applies to project-controlled TG SDK code. O3DE, operating systems, game platforms, third-party tools, dependencies, and runtime adapters may have their own behavior and policies.

## Data the editor may handle locally

Depending on user configuration, local data may include:

- game installation paths and version metadata;
- BepInEx and assembly paths;
- workspace and deployment paths;
- pack ownership and compatibility declarations;
- diagnostic exports, logs, screenshots, dumps, and research notes;
- source fingerprints;
- evidence, catalog, validation, and build documents;
- generated packages and reports.

Some of this data may contain personal usernames, machine paths, mod names, or proprietary game information.

## Collection principles

Project-controlled features must:

- collect only data required for the user-requested local workflow;
- explain remote or external communication before it occurs;
- require explicit opt-in for future telemetry;
- avoid persistent machine or user identifiers when not necessary;
- provide a way to disable and remove optional collected data;
- never sell user data;
- never send workspace or game data to a remote service silently.

## Logs and diagnostics

Logs should avoid unnecessary personal paths, environment variables, credentials, tokens, and proprietary content.

Before sharing logs or screenshots:

- remove usernames and home-directory paths;
- remove account identifiers and tokens;
- remove private repository or mod information;
- remove proprietary game data not necessary to reproduce the issue;
- reduce the report to the smallest relevant excerpt.

## Source fingerprints

SHA-256 fingerprints are used to identify imported artifacts and detect duplicates. A fingerprint may reveal that two users possess identical data, but it does not contain the original file content.

Do not treat a fingerprint as anonymous when the underlying file is unique, sensitive, or publicly identifiable.

## Remote integrations

A future remote service, update checker, crash reporter, collaboration feature, or package registry requires design and privacy review before implementation.

The design must document:

- controller/operator of the service;
- data sent;
- purpose and retention;
- authentication and security;
- user consent and disablement;
- deletion and export controls;
- applicable third-party terms;
- offline behavior.

## Contributions

Do not commit:

- personal workspace documents;
- unsanitized diagnostics;
- access tokens or credentials;
- private URLs;
- another person's data;
- telemetry identifiers;
- logs containing protected or confidential content.

Use synthetic fixtures in tests.

## Security incidents

Privacy defects and unintended disclosure follow [SECURITY.md](../../SECURITY.md). Do not expose affected data in a public issue.
