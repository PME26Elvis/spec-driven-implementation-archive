# Task Pack Changelog

## v1.0 Windows Edition — 2026-08-08

Created a Windows-native parity edition of the frozen Linux/X11 v1.0 assignment.

The product feature scope remains intentionally equivalent: Markdown editing/rendering, rendered editing, workspace/tabs, images/Base64/assets, history/diff, recovery, Command Palette, preferences, custom UI effects, performance/failure gates, and all three authored development utilities remain mandatory.

Windows-specific changes include:

- Replaced the X11 platform boundary with native Win32/Windows SDK integration.
- Frozen a low-level User32 + application-owned software-rendering boundary; GDI/DIB presentation is permitted.
- Explicitly prohibited WinUI, WPF, Windows Forms, MFC, Rich Edit/EDIT, native TreeView/Tab/common-control substitution, WebView2, CEF, and comparable shortcuts.
- Permitted Windows system file/folder pickers only as a narrow OS-integration exception.
- Permitted WIC solely for PNG/JPEG/BMP codec decode/encode.
- Permitted DirectWrite/GDI solely for system glyph shaping/rasterization without delegating editor or Markdown layout.
- Added mandatory DPI awareness and 100%/150%/200% scaling acceptance.
- Added Unicode Win32 boundary requirements and prohibition on ANSI-code-page-dependent mandatory paths/text.
- Defined Windows `CF_UNICODETEXT` clipboard interoperability.
- Defined Windows IME composition/commit/cancel/candidate placement requirements.
- Defined Explorer-compatible file drag/drop acceptance.
- Defined Windows per-user Roaming AppData preferences/recents and Local AppData recovery locations.
- Defined Windows path identity, invalid/reserved filenames, case-insensitive NTFS acceptance semantics, and >260-character long-path coverage.
- Replaced symlink-only traversal semantics with Windows directory reparse-point/junction behavior.
- Defined Windows-safe Save staging/flush/replace semantics and sharing-violation behavior.
- Added external-change constraints that avoid unnecessarily exclusive document handles.
- Added Windows-specific evidence path security for drive-letter, UNC, extended-namespace, rooted, traversal, and reparse escapes.
- Updated `locscan`, `fixturegen`, and `evidencecheck` requirements for Unicode Windows paths and reparse-point handling.
- Added `docs/16_WINDOWS_PLATFORM_CONTRACT.md` as a normative Windows-specific contract.
- Updated Release Gates and acceptance matrices to require Windows clipboard, Explorer drag/drop, IME, DPI, long-path, reserved-name, sharing-violation, and reparse-point coverage.

No agent/tool/MCP/browser/search/package-install/screenshot-capture workflow is prescribed by this edition.

## Source Baseline

This Windows edition was ported from the feature-complete v1.0 assignment dated 2026-08-07. Feature omissions are not permitted merely because a Linux/X11 integration point required a Windows equivalent.
