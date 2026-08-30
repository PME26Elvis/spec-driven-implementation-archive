# 11 — Delivery Requirements and Forbidden Substitutions

## 1. Required source delivery

Deliver the complete source tree required to build Project A and Project B.

At minimum include:

- C source files;
- headers;
- build files;
- test source;
- test fixtures;
- example JSON/YAML configs;
- documentation;
- any embedded font/glyph source data required for UI;
- scripts only where optional convenience is useful, not as replacements for required C tools.

## 2. Required documentation

Submission documentation must explain:

- build command;
- Project A usage;
- Project B launch options;
- config path behavior;
- deterministic validation mode;
- test command;
- known non-mandatory limitations if any;
- where validation artifacts are written.

## 3. Binary delivery

Prebuilt binaries may be included optionally, but source buildability remains mandatory.

The submission must not consist only of binaries.

## 4. Generated/build artifacts

Do not treat compiler objects, caches, screenshots, logs, generated reports, or temporary files as authored source.

They should normally be excluded from the clean deliverable unless specifically included as small validation evidence.

## 5. Forbidden GUI framework substitution

It is prohibited to implement the required UI using GTK, Qt, SDL widgets, browser/webview content, Electron, Tk, ncurses, or another GUI toolkit.

A browser-rendered HTML clock inside a window does not satisfy the task.

## 6. Forbidden native-widget substitution

X11/native child controls or system dialogs must not replace application-rendered buttons, sliders, text fields, navigation, settings controls, or modal cards.

## 7. Forbidden renderer substitution

Do not use Cairo, Skia, OpenGL/GLX, Vulkan, XRender, MIT-SHM rendering shortcuts, image-processing libraries, Xft/FreeType, or another raster/vector library to implement the required software-rendering work. Submitted code must not directly call XCB; Xlib's own transitive implementation dependencies do not count as submitted API use.

## 8. Forbidden pre-rendered UI

Do not render entire UI states as prepared PNG/JPEG/BMP images and merely switch them on interaction.

Decorative optional assets are allowed only when core components are still rendered/interactive as specified.

## 9. Forbidden fake blur

A translucent overlay by itself is not background blur.

The project must perform real spatial filtering/approximation on application content for required blur effects.

## 10. Forbidden fake animation

Instant state changes with sleeps inserted before/after do not satisfy interpolated animation requirements.

Animations must update visual state over elapsed time.

## 11. Forbidden fake hand dragging

Dragging must use pointer geometry relative to the clock face.

Do not implement hand dragging as hidden slider manipulation, canned sequences, fixed increments unrelated to pointer angle, or click-to-jump only.

## 12. Forbidden independent clocks

The analog and digital displays may not each maintain their own time and periodically copy values between them.

One canonical source of truth is required.

## 13. Forbidden system-time shortcut

Do not initialize, reset, periodically synchronize, or continuously derive canonical simulated time from system wall-clock/calendar time. The simulator starts from configured/persisted state and advances its own state using monotonic elapsed intervals and playback rate. Reading a monotonic duration source for animation/simulation scheduling is explicitly allowed and is not wall-clock synchronization.

## 14. Forbidden positive-only shortcut

Negative playback is mandatory.

Clamping negative slider values to zero, visually showing negative while advancing forward, or disabling reverse wrapping violates the task.

## 15. Forbidden undo shortcut

Undo/Redo may not be limited to digital text edits only.

All mandatory action classes must participate.

A history implementation that stores one entry for every pointer-move event violates gesture transaction requirements.

## 16. Forbidden configuration libraries

JSON/YAML parsing must not be delegated to external libraries, subprocesses, scripting runtimes, or system utilities.


## 16A. Forbidden dynamic/external algorithm substitution

Mandatory functionality may not be obtained by `dlopen`/runtime loading of a forbidden library, invoking helper processes, shelling out to desktop utilities, or communicating with an external local service. The same restriction applies even if the build does not link the forbidden dependency directly.

## 17. Forbidden YAML-as-JSON shortcut

YAML support cannot mean accepting only JSON syntax in `.yaml` files.

The mandatory YAML block syntax subset must work.

## 18. Forbidden regex-only parser claim

A set of line regexes that cannot correctly handle nesting/quoted strings does not satisfy JSON/YAML parser requirements.

The implementation needs lexical/syntactic state sufficient for the specified grammar.

## 19. Forbidden silent unknown keys

Misspelled settings must not be ignored silently.

Unknown-key rejection is mandatory.

## 20. Forbidden hard-coded test answers

Tests and validation tools must compute results from provided inputs.

Do not special-case fixture filenames or expected known examples to print passing outputs.

## 21. Forbidden mock Project A

`locscan`, `cfgcheck`, and `stateprobe` must be functional C programs.

Shell wrappers around unavailable/non-submitted tools do not satisfy Project A. `locscan` must perform its own line counting, classification, and specified glob matching; invoking `wc`, `find`, `glob(3)`, `fnmatch(3)`, regex libraries, or similar facilities as the implementation of those required algorithms is prohibited. POSIX directory traversal and metadata calls are allowed substrate operations.

## 22. Forbidden unconnected settings

Every mandatory GUI setting must influence the real corresponding runtime behavior.

Changing a slider label without changing blur/animation/etc. is insufficient.

## 23. Forbidden non-persistent Apply

If Apply reports configuration saved, the on-disk file must actually contain a valid representation of the applied settings.

## 24. Forbidden destructive save

Directly truncating the only valid config file before successful serialization/write completion violates atomic-save requirements.

## 25. Forbidden screenshot-only acceptance

Screenshots alone cannot establish correctness.

Functional tests and human interaction checklist are both required.

## 26. Forbidden omitted errors

The implementation may not terminate on every recoverable input/config error merely to avoid building error UI.

Runtime reload/save/input validation errors require application behavior described in the specs.

## 27. Forbidden placeholder/TODO behavior

Mandatory functions must not contain placeholder implementations that always return success, fixed values, or `TODO` behavior.

Comments mentioning future optional enhancements are fine if mandatory behavior is complete.

## 28. Forbidden dead controls

Visible controls that imply supported mandatory behavior but are not wired are release blockers.

## 29. Forbidden debug-only success

A feature that works only in a special hard-coded demo path but not under ordinary UI interaction is incomplete.

Deterministic mode must exercise real paths.

## 30. Forbidden omission by documentation

An implementation cannot declare a mandatory feature 'unsupported' in its README to bypass it.

Only features explicitly marked optional in this task pack may be omitted.

## 31. Delivery cleanliness

The final package should exclude:

- compiler object files unless specifically needed for evidence;
- `.git` internals;
- editor caches;
- core dumps;
- huge generated logs;
- temporary files;
- dependency caches.

## 32. Licensing/source provenance

If small public-domain/permissively licensed data such as a bitmap glyph table is incorporated, its origin and license must be documented.

Copying a complete third-party UI/rendering/parser implementation defeats the hand-built requirement and is prohibited even if its license permits reuse.
