# Kandra Golden Rules

Status: derivative reference. These rules summarise existing owner and FOA-SDK governance; controlling sources remain authoritative.

## Scope and authority

1. The current explicit owner instruction controls the task scope.
2. Repository process controls how the authorised task is executed; it does not authorise extra work.
3. A recommendation, TODO, roadmap item, report conclusion, or "Next researched task" is not authority to execute it.
4. Research permission is not repository-write permission.
5. Repository write, commit, push, pull request, merge, deployment, signing, publication, and release are distinct transitions. Perform only the transition currently authorised.
6. Normal repository delivery uses a focused non-`main` branch and pull request. Direct-`main`, merge, release, deployment, signing, settings changes, and protected external mutation require explicit authority where repository policy says so.
7. Do not broaden a task into cleanup, redesign, a later milestone, or another research question without authority.

## Research

8. Research is conditional, not a universal precondition.
9. When the owner names a research method, that method is binding.
10. ChatGPT Deep Research means actual ChatGPT Deep Research. A brief, web search, repository inspection, ordinary synthesis, or manual investigation is not a substitute.
11. If required Deep Research cannot execute, report the exact blocker. Do not manufacture a replacement report.
12. A returned Deep Research report is E1 research context until consequential claims are reviewed against their underlying evidence.
13. Preserve original research inputs unchanged. Corrections and cleaned versions are separate derivatives.
14. A cleaned derivative must stay labelled as a derivative and retain a link to the preserved input.
15. Replace conversation-local or opaque citations with durable source-register entries before relying on a consequential claim.
16. Bind repository observations to exact paths and revisions; bind external claims to direct sources, versions/revisions where available, and retrieval/observation dates.
17. Keep facts, inferences, proposals, contradictions, unknowns, and superseded claims distinct.
18. Do not automatically execute a report's "Next researched task".

## Evidence

19. One evidence lane cannot substitute for another.
20. Repository/static evidence is not live runtime proof.
21. Public research is not decompilation/static proof.
22. Decompilation/static evidence is not live execution proof.
23. Configure/build success is not Editor/UI interaction proof.
24. Adapter compilation is not Fall of Avalon compatibility proof.
25. Runtime behavior is not automatically persistence/save proof.
26. A hash or validation receipt proves binding/integrity properties only; it does not prove human authorization or source authenticity by itself.
27. Repository evidence does not prove the state of the user's installed game.
28. Unknown fields stay unknown. Do not fill them with defaults, guesses, nearby values, derived-product metadata, or convenient assumptions.
29. Negative searches are bounded limitations, not universal proof of absence.
30. Evidence must be bound to the exact source/ref/artifact that was actually tested.

## Validation and status

31. Use only these execution/result states: `PASSED`, `FAILED`, `PARTIAL`, `BLOCKED`, `NOT_RUN`, `NOT_APPLICABLE`.
32. Pending is not passing.
33. Skipped, absent, stale-head, wrong-commit, or zero matching tests are not passes.
34. Report what actually ran, not what should have run.
35. Record unavailable required evidence as `PARTIAL`, `BLOCKED`, or `NOT_RUN`; record irrelevant evidence as `NOT_APPLICABLE`.
36. Do not upgrade static validation into compiled, Editor, runtime, deployment, signing, or release proof.

## Promotion

37. Research does not create implementation authority.
38. R5/RH5 material is a promotion candidate until a human owner accepts it into a normative source.
39. Agents cannot be the final promotion owner.
40. Promotion does not itself authorise implementation, merge, deployment, signing, publication, or release; those transitions still require their own authority.
41. Contradictions and adverse evidence stay visible through promotion review.

## Protected material

42. Do not commit secrets, credentials, private paths, saves, signing material, proprietary commercial content, or external game source/assets without redistribution rights.
43. Read-only protected evidence stays outside the repository unless an authorised policy explicitly permits otherwise.
44. Redact private paths, credentials, user data, and protected payloads from logs, screenshots, receipts, and reports.

## Truth discipline

45. Truth takes precedence over completion.
46. Never claim a file was changed unless it was changed and verified.
47. Never claim a commit was pushed unless the ref was updated and verified.
48. Never claim a pull request was merged unless the merge occurred and was verified.
49. Never claim a command or test ran unless it actually ran.
50. Never convert generated text into a claimed repository change, static evidence into runtime proof, or research context into accepted project truth.
