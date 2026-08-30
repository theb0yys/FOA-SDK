# Support

The Tainted Grail Modding Editor and SDK is a community project in pre-alpha development. Support is best-effort and depends on maintainer and contributor availability.

## Where to ask

Use GitHub issues for reproducible bugs and scoped feature requests. Use the repository issue forms so reports include the information needed for triage.

Before opening an issue:

1. read the [User Guide](docs/tainted-grail-sdk/USER_GUIDE.md);
2. check existing open and closed issues;
3. confirm the problem occurs on the current `main` or `foa-development` state as appropriate;
4. remove secrets, personal paths, and copyrighted game content from logs and screenshots;
5. collect the exact workspace, game-profile, pack, importer, and build details relevant to the problem.

## Supported questions

The project can help with:

- building and loading the TG SDK Gem in a supported O3DE source environment;
- configuring a workspace and exact game profile;
- creating and validating pack manifests;
- importing source and evidence documents;
- understanding blockers, data formats, and project boundaries;
- reproducing editor crashes or persistence errors caused by this project;
- contributing code, documentation, tests, or research-safe fixtures.

## Not supported

The project does not promise support for:

- general O3DE installation problems unrelated to this Gem;
- the game's own bugs, crashes, balance, or account support;
- pirated, modified, or unsupported game installations;
- private modpacks or binaries that cannot be reproduced from source;
- requests to bypass copy protection, platform restrictions, or game security;
- recovery of corrupted saves without a project-caused reproducible defect;
- proprietary game assets or decompiled code that cannot legally be shared;
- unsafe direct runtime mutations outside the adapter and permission model.

## Bug reports

A useful bug report includes:

- project commit or release;
- operating system;
- compiler and O3DE build configuration;
- exact FoA profile version, branch, and runtime target when relevant;
- steps to reproduce from a clean state;
- expected and actual behavior;
- logs with sensitive information removed;
- whether workspace files, game files, deployments, or saves changed;
- minimal sample data that is legal to redistribute.

## Feature requests

Feature requests should explain:

- the user problem;
- the proposed workflow;
- affected architecture and data formats;
- safety, compatibility, persistence, and migration concerns;
- evidence or research supporting FoA-specific assumptions;
- acceptance criteria.

Large changes require design review before implementation.

## Security and conduct

Security vulnerabilities must follow [SECURITY.md](SECURITY.md), not public bug-reporting channels.

Community behavior must follow [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Response expectations

No response-time guarantee is offered. Maintainers may close issues that are incomplete, unreproducible, out of scope, unsafe, legally unclear, duplicates, or inactive after requests for information.
