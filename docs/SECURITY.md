# Security Policy

## Scope

This policy covers the Tainted Grail Modding Editor and SDK code, documentation, data formats, importers, build tooling, and project-controlled workflows in this repository.

It also covers security and safety defects that could cause:

- arbitrary code execution;
- unsafe file writes or path traversal;
- secret or personal-data disclosure;
- malicious dependency execution;
- corrupted workspaces, packages, deployments, or saves;
- silent modification of a game installation;
- bypass of validation, permission, or runtime-boundary controls;
- untrusted input causing denial of service or resource exhaustion;
- unsafe handling of diagnostic dumps, logs, screenshots, or extracted data.

## Supported versions

The project is pre-alpha. Security fixes are applied to the current `main` branch. Older commits, local forks, and unpublished builds are not supported unless a release explicitly states otherwise.

## Reporting a vulnerability

Do **not** open a public issue or pull request containing vulnerability details, proof-of-concept payloads, private game data, secrets, or paths that identify another person.

Preferred reporting method:

1. Open the repository's **Security** tab.
2. Use **Report a vulnerability** or private vulnerability reporting when available.
3. Include a clear description, affected commit or version, impact, reproduction steps, and any proposed mitigation.

When private vulnerability reporting is unavailable, open a minimal public issue requesting a private security contact. Do not include sensitive details in that issue.

## What to include

A useful report contains:

- affected component and commit;
- operating system and build configuration;
- trusted and untrusted inputs involved;
- exact reproduction steps;
- expected and actual behavior;
- security or safety impact;
- whether game files, user files, deployments, or saves were modified;
- logs or screenshots with secrets and personal paths removed;
- suggested mitigation, when known.

## Response process

Maintainers will, as capacity allows:

1. acknowledge the report;
2. assess scope and severity;
3. reproduce the issue in a controlled environment;
4. determine whether coordinated disclosure is needed;
5. prepare a fix and regression test;
6. publish an advisory and release note when appropriate.

Timelines depend on severity, reproducibility, maintainer availability, and upstream dependencies. Reporters should not assume a fix is complete until the maintainer confirms it.

## Disclosure expectations

Please allow reasonable time for investigation and remediation before public disclosure. Maintainers will credit reporters who request credit and whose reports materially improve the project.

## Security design rules

Contributions must preserve these defaults:

- editor and knowledge layers do not execute FoA gameplay actions;
- runtime actions are opt-in, adapter-owned, and independently validated;
- imports are bound to an exact game profile and fingerprinted;
- untrusted input is size-limited, schema-checked, and fail-closed;
- file writes remain inside explicit workspace, staging, output, or deployment roots;
- path traversal and symlink escapes must be considered;
- secrets and credentials must never be stored in workspace or pack documents;
- logs and diagnostics must avoid personal paths and proprietary content where possible;
- deployment and save-impact features require explicit confirmation and rollback planning;
- dependencies require licence, provenance, maintenance, and vulnerability review.

## Out of scope

The following are generally outside project security support:

- vulnerabilities in an unmodified upstream O3DE build, which should be reported upstream;
- vulnerabilities in the game, BepInEx, Harmony, Unity, operating systems, drivers, or third-party tools;
- unsafe behavior caused by locally modified builds that cannot be reproduced from this repository;
- cheating, balance issues, or normal mod incompatibilities without a security or data-loss impact;
- reports requiring distribution of copyrighted game assets or proprietary source material.

We may still help route a good-faith report to the appropriate upstream project.
