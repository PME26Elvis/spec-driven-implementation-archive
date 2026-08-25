# Live Chord Recognition Specification

## 1. Input Semantics

The recognizer receives the currently **physically/logically held note instances** after effective pitch was captured at note-on.

It does not include voices that continue only because of sustain or release tails.

For matching:

- duplicate pitch classes collapse to one pitch-class bit;
- the lowest held effective pitch remains available as bass information.

No input notes -> `—`.

One or two distinct pitch classes -> `—`.

---

## 2. Pitch-Class Naming

Sharp spelling is fixed:

0 C  
1 C#  
2 D  
3 D#  
4 E  
5 F  
6 F#  
7 G  
8 G#  
9 A  
10 A#  
11 B

No automatic flat spelling is required.

---

## 3. Chord Template Vocabulary and Labels

For every root `R`, normalize intervals modulo 12.

The exact required templates and suffixes are:

| Quality | Intervals | Label for root C |
|---|---|---|
| major | 0,4,7 | `C` |
| minor | 0,3,7 | `Cm` |
| diminished | 0,3,6 | `Cdim` |
| augmented | 0,4,8 | `Caug` |
| sus2 | 0,2,7 | `Csus2` |
| sus4 | 0,5,7 | `Csus4` |
| dominant 7 | 0,4,7,10 | `C7` |
| major 7 | 0,4,7,11 | `Cmaj7` |
| minor 7 | 0,3,7,10 | `Cm7` |
| minor-major 7 | 0,3,7,11 | `Cm(maj7)` |
| half-diminished 7 | 0,3,6,10 | `Cm7b5` |
| diminished 7 | 0,3,6,9 | `Cdim7` |
| major 6 | 0,4,7,9 | `C6` |
| minor 6 | 0,3,7,9 | `Cm6` |
| add9 | 0,2,4,7 | `Cadd9` |
| minor add9 | 0,2,3,7 | `Cmadd9` |
| dominant 9 | 0,2,4,7,10 | `C9` |
| major 9 | 0,2,4,7,11 | `Cmaj9` |
| minor 9 | 0,2,3,7,10 | `Cm9` |

All 19 templates are required for all 12 chromatic roots.

---

## 4. Exact-Match Rule

A candidate chord matches only when the distinct held pitch-class set exactly equals the template's pitch-class set after root transposition.

Extra notes invalidate the match unless they form another listed exact template.

Examples:

- C E G -> C;
- C D E G -> Cadd9;
- C E F# G -> `—`.

Omitted tones are not inferred.

---

## 5. Root Search and Ambiguity Resolution

Test all 12 pitch classes as possible roots.

Collect every exact template match.

If more than one remains, select in this fixed order:

1. candidate whose root pitch class equals the lowest held note's pitch class;
2. if still tied, template priority list in Section 6;
3. if still tied, numerically lowest root pitch class 0..11.

This rule intentionally favors a bass-root interpretation for symmetric/ambiguous pitch sets.

---

## 6. Template Priority

For the final tie-break, priority from highest to lowest is:

1. major 9;
2. minor 9;
3. dominant 9;
4. major 7;
5. minor-major 7;
6. minor 7;
7. dominant 7;
8. half-diminished 7;
9. diminished 7;
10. major 6;
11. minor 6;
12. add9;
13. minor add9;
14. major;
15. minor;
16. diminished;
17. augmented;
18. sus2;
19. sus4.

Because exact pitch-class sets usually differ, this table is primarily for theoretical ambiguity and must nevertheless be implemented deterministically.

---

## 7. Inversions / Slash Bass

After selecting a chord root:

- if lowest held pitch class equals root, display the base label only;
- otherwise append `/BassName` using the fixed sharp spelling.

Examples:

- E3 G3 C4 -> `C/E`;
- G3 C4 E4 -> `C/G`;
- B2 D3 F3 G3 -> `G7/B` if that set is G7 with B bass.

Duplicate root notes in upper octaves do not change the bass.

---

## 8. Duplicate Octaves

Duplicate pitch classes never prevent an exact match.

Examples:

- C3 C4 E4 G4 C5 -> `C`;
- E3 G3 C4 C5 -> `C/E`.

---

## 9. Event Timing

Chord recognition recomputes synchronously after the logical held-note set changes because of:

- note-on;
- note-off;
- pointer capture loss;
- focus-loss forced keyboard release;
- all-notes-off/shutdown cleanup.

There is no required debounce timer.

Transpose/octave control changes alone do not alter existing held note instances and therefore do not change the current chord until new note events change the held set.

---

## 10. Sustain

A physical/logical input release removes that note from the chord held set immediately, even when its audio voice remains sustain-latched.

Example:

1. Sustain on;
2. hold C E G -> `C`;
3. release E while sound continues;
4. chord becomes `—` immediately.

---

## 11. Mandatory Exhaustive Tests

The test suite shall generate table-driven tests for:

- all 19 templates;
- all 12 roots;
- root position for every template/root pair: 228 cases minimum.

In addition, for every template with `k` distinct pitch classes, the suite shall test each possible chord-tone bass inversion at least once for root C or another fixed reference root.

Required explicit regression cases are listed below.

### CR-001 C Major
C4 E4 G4 -> `C`

### CR-002 A Minor
A3 C4 E4 -> `Am`

### CR-003 C First Inversion
E3 G3 C4 -> `C/E`

### CR-004 C Second Inversion
G3 C4 E4 -> `C/G`

### CR-005 G7
G3 B3 D4 F4 -> `G7`

### CR-006 Cmaj7
C4 E4 G4 B4 -> `Cmaj7`

### CR-007 Am7
A3 C4 E4 G4 -> `Am7`

### CR-008 Bdim
B3 D4 F4 -> `Bdim`

### CR-009 Caug
C4 E4 G#4 -> `Caug`

### CR-010 Dsus2
D4 E4 A4 -> `Dsus2`

### CR-011 Dsus4
D4 G4 A4 -> `Dsus4`

### CR-012 B Half-Diminished
B3 D4 F4 A4 -> `Bm7b5`

### CR-013 B Diminished Seventh
B3 D4 F4 G#4 -> `Bdim7`

The pitch set is symmetric; bass-root rule selects B.

### CR-014 C6
C4 E4 G4 A4 -> `C6`

### CR-015 Am6
A3 C4 E4 F#4 -> `Am6`

### CR-016 Cadd9
C4 D4 E4 G4 -> `Cadd9`

### CR-017 C9
C3 E3 G3 A#3 D4 -> `C9`

### CR-018 Duplicate Octaves
C3 C4 E4 G4 C5 -> `C`

### CR-019 Inversion With Duplicate Root
E3 G3 C4 C5 -> `C/E`

### CR-020 Empty
no notes -> `—`

### CR-021 One Note
C4 -> `—`

### CR-022 Two Notes
C4 E4 -> `—`

### CR-023 Unsupported Extra Note
C4 E4 F#4 G4 -> `—`

### CR-024 Immediate Release Update
Hold C4 E4 G4 -> `C`; release E4 -> `—` in the same logical update cycle.

### CR-025 Sustain Exclusion
Sustain on; hold C E G -> `C`; release E -> `—` while E may remain audible.

### CR-026 New Notes After Transpose
Set transpose +2; newly press displayed C E G positions -> effective D F# A -> `D`.

### CR-027 Existing Notes Not Retuned
Hold C E G at transpose 0 -> `C`; change transpose to +2 without releasing -> remains `C`.

### CR-028 Rapid Replacement
C E G -> release all -> D F A -> final `Dm`, no stale `C`.

### CR-029 Mixed Sources
Pointer C + keyboard E/G -> `C`; release pointer C -> `—`.

### CR-030 Focus Loss
Keyboard holds C/E/G -> `C`; focus loss clears keyboard-held notes -> `—`.

### CR-031 C Major Ninth
C3 D3 E3 G3 B3 -> `Cmaj9`.

### CR-032 C Minor Ninth
C3 D3 D#3 G3 A#3 -> `Cm9`.

### CR-033 C Minor-Major Seventh
C3 D#3 G3 B3 -> `Cm(maj7)`.

### CR-034 C Minor Add9
C3 D3 D#3 G3 -> `Cmadd9`.

---

## 12. Testability

The recognizer shall be a pure/deterministic core module callable without GUI or audio initialization.

It shall return a structured result containing at least:

- matched boolean;
- root pitch class when matched;
- bass pitch class;
- quality/template ID;
- canonical label.
