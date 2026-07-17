# Accessibility Standard

The TG SDK editor should be usable by people with varied vision, mobility, hearing, cognition, language, and input needs.

## Requirements for editor tools

- Every input has a visible, meaningful label.
- Keyboard navigation and tab order are logical.
- Buttons describe the action they perform.
- Status, warnings, and errors are not communicated by color alone.
- Text has sufficient contrast within the host theme.
- Long paths and messages can be selected and copied.
- Controls remain usable with UI scaling and high-DPI displays.
- Tables have clear column headings and stable reading order.
- Dynamic updates do not unexpectedly steal focus.
- Destructive or high-impact actions require clear confirmation.
- Error messages explain recovery without relying on icons alone.

## Language

- Use direct, concrete wording.
- Define project-specific terms.
- Avoid unnecessary abbreviations.
- Separate errors, warnings, and informational status.
- Explain why an action is blocked and what the user can do next.
- Do not use jokes or blame in error messages.

## Motion and timing

- Avoid unnecessary animation.
- Do not require rapid input.
- Long-running operations should expose progress and cancellation when practical.
- Do not dismiss important messages before the user can read them.

## Screen readers and Qt semantics

When practical:

- provide accessible names and descriptions;
- associate labels with controls;
- use standard Qt widgets before custom-painted controls;
- expose meaningful table and status semantics;
- test focus and reading order.

## Review evidence

User-interface pull requests should include:

- keyboard-only interaction steps;
- screenshots at normal and scaled UI when layout changed materially;
- confirmation that severity is not color-only;
- notes for any known accessibility limitation.

## Reporting accessibility defects

Use the TG SDK bug form and select the affected editor tool. Describe the assistive technology, scaling, keyboard path, or readability issue without including private information.
