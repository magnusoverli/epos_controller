# UI polish — root causes and fixes

Reference: the Windows settings screenshots under
`captures/windows/epos-evidence-20261012-135151-reviewed/screenshots/`, including
`05-original-settings-top.png`. The existing restrained feature-table layout is
retained; the work focuses on interaction and feedback rather than copying the
Windows implementation.

| Problem | Root cause | Fix |
|---|---|---|
| One setting freezes the whole page and clears values | A global worker-busy flag controlled every editor; starting a write discarded the entire snapshot | Keep confirmed values separate from requested edits; update the UI immediately and serialize device operations in a bounded, latest-value queue |
| Rapid edits are ignored or can overwrite each other | Calls made during busy were discarded; there was no representation of waiting user intent | Coalesce waiting edits per setting; rebase each dispatch on confirmed predecessor readback; cancel pending edits on failure or connection change |
| Slider jumps during unrelated readback | Every `control` map change reset `slider.value` | Synchronize only the relevant value and explicit refresh/connection revision; preserve an active drag |
| Sidetone needs an extra Apply action | The slider used a separate draft-and-button workflow | Mouse/touch release commits once; no Apply button, keyboard adjustment, or per-motion USB writes |
| Disabled ANC controls lack an explanation | Dependency rules were duplicated between controller and domain validation, with no reason exposed to QML | Share prerequisite validation; expose the reason through tooltips and accessibility descriptions |
| Failure is only shown in a global message | Per-setting state was absent | Show an error at the affected editor and retain last known values; canceled waiting edits are identified and are not automatically retried |
| Settings toolbar changes width/position while editing | “Refresh” changed to “Working…” and status text altered layout height | Stable button label and status area; no settings progress indicators or save-success notifications |
| Compact windows show too few settings and misalign editors | Large fixed padding/toolbars consumed the viewport; a nested layout expanded beyond its preferred column width | Compact spacing, Refresh in the header, error-only status after loading, and a fixed shared editor-column width |
| Dead Restore Settings action | A disabled placeholder survived after confirmed Factory Reset was implemented | Remove the duplicate placeholder; retain the explicit reset action |
| Section headers suggest expansion but do nothing | Headings were static labels with a disclosure glyph | Use focusable toggle headers with clear arrows; sections start collapsed and retain their expansion state during navigation |
| Unnecessary notification testing action | A diagnostic action occupied everyday settings space | Remove Test notification from the UI |
| Firmware identity disappears at compact widths | The label lived inside the hidden artwork sidebar | Keep firmware identity in the settings header at all supported widths |
| Navigation requires guessing icons | Labels existed only in hover tooltips/accessibility metadata | Add persistent navigation labels while retaining focus treatment |
| Update UI gives contradictory messages during reboot | Catalog/disconnection copy remained visible alongside installer status | Prefer installation status while installing; keep its navigation entry visible during reconnection; format check times locally |
| Help describes outdated functionality | Static scope text predates firmware installation | Update feature descriptions |

## Separation of concerns

- `settings/EditQueue.h`: Qt-free intent coalescing; no USB or UI behavior.
- `settings/SettingsTypes.*`: shared dependency and value validation.
- `SettingsPresentation.*`: choices and lossless display formatting only.
- `SettingsController.*`: asynchronous orchestration, connection boundaries,
  confirmed snapshots and error presentation; transport remains injected.
- `qml/SettingEditor.qml`: reusable dropdown/slider/error presentation.
- `qml/PercentSetting.qml`: pointer gesture state; no device protocol knowledge.

Settings verification remains in `Settings::Session`: fresh preflight, one
setter, and complete readback. A responsive editor does not mean concurrent USB
writes or treating requested values as confirmed device state.

Tests hold the transport worker to exercise editing during latency, coalescing,
dependencies, cancellation, failure and reconnection. Rendered UI tests cover
pointer release, a drag surviving unrelated readback, refresh invalidation,
keyboard navigation of the other controls, and normal/high-DPI layouts. No live
headset writes are needed for this review.

Validation passed: 14 Release and 14 ASan/UBSan suites, normal/high-DPI rendered
UI checks, QML lint, clang-format 21, relocated-install startup and `git diff --check`.
