# Installer packaging

This directory contains source projects that convert one already verified staging payload into platform-specific installer formats.

Packaging projects do not select arbitrary files from a source checkout or raw build directory. Inventory selection, redistribution-review binding, canonical staging, checksums, provenance, notices, SBOM generation, portable archives, and verification remain explicit gates.

Generated packages are written outside the source checkout. Packaging source does not grant signing, publication, deployment, game-launch, runtime, or save authority.
