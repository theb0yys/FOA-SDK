# Canonical Source Fact — Tainted Economy Diagnostic Writer

```yaml
fact_id: tg.fact.items-economy.diagnostic-writer
status: source-verified-adverse
scope: pinned upstream source only
profile_id: foa-1.23.401-mono-bepinex5-tf-0.1.33
source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
source_commit: d7e740e7f167b73152b53409e483dab07d80d048
source_paths:
  - mods/TaintedEconomy/src/Diagnostics/EconomyDiagnosticWriter.cs
  - mods/TaintedEconomy/src/Diagnostics/EconomyDiagnosticRow.cs
source_blobs:
  - 020dd5c48f691690b5854446e56c0a5f4051bc3b
  - 6e0711c180db1a7c0e464914b0854987d5f4a840
decision: prohibited-current-source
```

## Verified source behavior

The writer:

- defines 26 CSV fields from UTC timestamp through action/mutation;
- applies a configurable **per-lane row-count cap**, clamped to at least one;
- sets action and mutation to `none` before serialization;
- writes one file per economy lane plus `diagnostic-summary.md`;
- optionally logs the complete CSV line to BepInEx;
- constrains the configured root with `Path.GetFullPath` followed by a case-insensitive string-prefix comparison;
- appends a sanitized session-name segment;
- quotes commas, quotes, carriage returns, and newlines using standard CSV quote doubling;
- increments a lane count only when the write/log path reports success.

## Deterministic adverse findings

### Root containment

A string-prefix comparison is not a canonical descendant check. A sibling path beginning with the same text as the config root can be accepted. The check also does not establish final-target containment across links or junctions.

### Session traversal

The session sanitizer replaces invalid filename characters but accepts `.` and `..`. Combining those segments can resolve to the root or its parent instead of a strict session child. Rooted, reserved, trailing-dot/space, device-name, and overlong segments do not have an explicit fail-closed contract.

### Publication atomicity

CSV append occurs before summary replacement. When append succeeds and summary writing fails, the method catches the exception before setting `wrote = true`. With row logging disabled, the durable row is not counted. Repeated attempts can therefore publish more rows than the intended row cap.

### Bounds and privacy

The source has no:

- per-field, per-row, per-file, or per-session byte cap;
- field classification or redaction policy;
- retention, expiry, deletion, or export lifecycle;
- CSV spreadsheet-formula neutralization;
- atomic session publish/rollback;
- unload reset or durable receipt proving what was written.

The row schema may include scene names, template identities, display names, tags, prices, arbitrary context, observed values, and values produced through reflected getters or `ToString`.

## Decision

The writer is a canonical source fact but is **prohibited-current-source** for approved diagnostics capture, support-safe evidence, or publication. The prohibition can be reconsidered only after the Batch 004 diagnostics fixture passes against a reviewed replacement implementation.

Fixture: [`../../hooks/fixtures/batch-004-diagnostic-writer.json`](../../hooks/fixtures/batch-004-diagnostic-writer.json)

No part of this fact authorizes filesystem access, diagnostics capture, spreadsheet opening, support sharing, or evidence promotion.
