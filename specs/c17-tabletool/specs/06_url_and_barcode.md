# 06 — URL and Barcode Algorithms

## Part A — HTTP/HTTPS URL type

## 1. Purpose and scope

URL is a deliberately bounded semantic type.
It is not a complete RFC 3986/3987 implementation.

Required URL support is limited to absolute HTTP and HTTPS URLs of the form:

```text
scheme://host[:port][/path][?query][#fragment]
```

where scheme is `http` or `https`.

The program validates and normalizes the URL.
It does **not** fetch it.

## 2. Character repertoire

A URL input must use ASCII bytes only, code points U+0021 through U+007E where allowed by component grammar.

Raw spaces and non-ASCII characters are rejected.
Internationalized domain names and raw Unicode paths are out of scope.

Non-ASCII octets may be represented textually using valid percent escapes such as `%E4%B8%AD`.
TableTool validates the percent-escape spelling but does not decode it to Unicode.

## 3. Scheme

Accepted schemes are ASCII-case-insensitive:

```text
http
https
```

The exact delimiter `://` is mandatory.

Examples:

```text
HTTP://example.com
https://example.com
```

Accepted scheme is canonicalized to lowercase.

Any other scheme is rejected, including:

```text
ftp:
mailto:
file:
ws:
```

## 4. Authority restrictions

The authority contains only:

```text
host
host:port
```

User information is not supported.
Therefore an authority containing `@` is rejected.

IPv6 address literals are not supported.
Therefore bracketed hosts such as `[::1]` are rejected.

## 5. DNS host syntax

A DNS-style host consists of labels separated by `.`.

Each label:

- is 1 through 63 ASCII characters;
- contains only `A-Z`, `a-z`, `0-9`, and `-`;
- must not begin with `-`;
- must not end with `-`.

The complete host must be no more than 253 characters.
An empty label is rejected.
A trailing dot is rejected for this assignment.

Examples accepted:

```text
example.com
WWW.Example.COM
a-b.example
localhost
```

Examples rejected:

```text
.example.com
example.com.
a..b
-a.example
a-.example
例子.example
```

DNS host letters are canonicalized to lowercase.

No DNS lookup is performed.

## 6. IPv4 host syntax

A host may instead be dotted decimal IPv4:

```text
d.d.d.d
```

Each component:

- contains 1 through 3 decimal digits;
- represents 0 through 255;
- must not contain a leading zero when it has more than one digit.

Accepted:

```text
127.0.0.1
192.168.1.10
0.0.0.0
```

Rejected:

```text
127.000.0.1
256.1.1.1
127.1
1.2.3.4.5
```

Canonical text uses ordinary decimal components.

A host that syntactically consists only of digits and dots but is not valid IPv4 must not be reinterpreted as a DNS host.

## 7. Port

Port is optional.

If present:

- colon is followed by 1 or more ASCII digits;
- value must be from 1 through 65535;
- signs and whitespace are forbidden.

Leading zeros are accepted and removed in canonical output.

Default ports are removed:

- `http:80` => no explicit port;
- `https:443` => no explicit port.

Non-default ports remain.

Examples:

```text
http://example.com:080/   -> http://example.com/
https://example.com:443/a -> https://example.com/a
https://example.com:8443/ -> https://example.com:8443/
```

## 8. Path, query, and fragment character validation

After authority, the path begins with `/` if present.

Allowed unescaped ASCII characters in path are:

```text
A-Z a-z 0-9 - . _ ~
! $ & ' ( ) * + , ; =
: @ /
```

Allowed unescaped ASCII characters in query and fragment are the same plus:

```text
?
```

A percent escape is allowed in path, query, or fragment and must be exactly:

```text
% HEX HEX
```

where HEX is 0-9, A-F, or a-f.

Malformed percent escapes are rejected.

In canonical output the two hex digits of each percent escape are uppercase.
Percent escapes are **not decoded**, even when they represent an unreserved character.

Raw `#` terminates query/path and begins fragment.
Raw `?` after authority/path begins query.
A second raw `#` inside fragment is rejected by this restricted grammar.

## 9. Empty path

If authority is followed immediately by:

- end-of-input;
- `?`;
- `#`;

the canonical path is `/`.

Examples:

```text
http://example.com        -> http://example.com/
http://example.com?x=1    -> http://example.com/?x=1
http://example.com#top    -> http://example.com/#top
```

## 10. Dot-segment removal

Literal path segments equal to `.` or `..` are normalized by the following deterministic rooted-segment algorithm.
Percent-encoded spellings such as `%2E` are ordinary segments and are never interpreted as dot segments.

Given a path that always begins with `/`:

1. remember `force_trailing_slash = true` if the original path ends with `/`, `/.`, or `/..`; otherwise false;
2. remove the one leading root `/` for tokenization purposes;
3. split the remainder on every `/`, **preserving empty segments**, including empty segments created by repeated slashes or a trailing slash;
4. process segments left to right with a stack:
   - segment `.`: discard it;
   - segment `..`: if the stack is non-empty, pop exactly one preceding stored segment; the popped segment may itself be empty; if the stack is empty, remain at root;
   - every other segment, including the empty string, is an ordinary segment and is pushed;
5. reconstruct `/` followed by the stored segments joined with `/`;
6. if the stack is empty, the reconstructed path is `/`;
7. if `force_trailing_slash` is true and the reconstructed path is not `/` and does not already end in `/`, append one `/`.

This means an empty segment from `//` **does count** as the nearest preceding ordinary segment that `..` can remove.
Repeated slashes are otherwise preserved.

Required examples:

```text
/a/./b          -> /a/b
/a/x/../b       -> /a/b
/../../a        -> /a
/a/%2e/b        -> /a/%2E/b
/a/b/..         -> /a/
/a//b           -> /a//b
/a//../b        -> /a/b
//../a          -> /a
/a///../../b    -> /a/b
/a/./../        -> /
/a//../         -> /a/
///a            -> ///a
```

An implementation may use a different internal algorithm only if it produces exactly the same canonical path for all inputs under these rules.

## 11. Query

Query begins at `?` and ends at `#` or end-of-input.

TableTool does not parse query parameters.
It does not reorder names, decode `+`, or normalize separators.

The query byte sequence is preserved except for uppercasing percent-escape hex digits.

An empty query marker is preserved:

```text
http://e.com/?  -> http://e.com/?
```

## 12. Fragment

Fragment begins at `#` and runs to end-of-input.

TableTool does not interpret it.

The fragment byte sequence is preserved except for uppercasing percent-escape hex digits.

An empty fragment marker is preserved:

```text
http://e.com/#  -> http://e.com/#
```

## 13. URL canonicalization order

A successful URL conversion performs, conceptually:

1. validate ASCII and top-level structure;
2. lowercase scheme;
3. parse/validate authority;
4. lowercase DNS host;
5. canonicalize IPv4 if applicable;
6. parse/canonicalize port and remove default port;
7. ensure path exists as `/`;
8. validate percent escapes;
9. uppercase percent-escape hex digits;
10. remove literal path dot segments;
11. preserve query and fragment content under the stated rules;
12. concatenate canonical components.

## 14. URL examples

Required examples:

```text
HTTP://Example.COM
=> http://example.com/

https://Example.COM:443/a/../b?Q=One#Part
=> https://example.com/b?Q=One#Part

http://127.0.0.1:80/a/%2f
=> http://127.0.0.1/a/%2F

https://example.com:00443
=> https://example.com/

https://example.com:8443
=> https://example.com:8443/
```

The following must fail:

```text
ftp://example.com/
https://user@example.com/
https://[::1]/
https://example.com/%ZZ
https://例子.com/
https://example.com:0/
https://example.com:65536/
```

---

# Part B — EAN-13

## 15. EAN13 accepted cell text

EAN13 accepts exactly:

- 12 ASCII decimal digits; or
- 13 ASCII decimal digits.

No spaces, hyphens, prefixes, or other characters are accepted.

For 12 digits the implementation computes and appends the check digit.
For 13 digits it verifies the supplied check digit.

Canonical text is always exactly 13 digits.

## 16. EAN-13 check digit

Number the first 12 digits from position 1 at the left.

Compute:

```text
S = (d1 + d3 + d5 + d7 + d9 + d11)
  + 3 * (d2 + d4 + d6 + d8 + d10 + d12)
```

Then:

```text
check = (10 - (S mod 10)) mod 10
```

For a 13-digit input, digit 13 must equal this check value.

Example:

```text
400638133393
```

has check digit `1`, so canonical EAN-13 is:

```text
4006381333931
```

## 17. EAN-13 symbol structure

The 13-digit number is encoded to exactly 95 barcode data/guard modules:

```text
start guard     3 modules
left six digits 42 modules
center guard    5 modules
right six       42 modules
end guard       3 modules
```

Guards:

```text
start  = 101
center = 01010
end    = 101
```

`1` is black bar and `0` is white space.

## 18. EAN digit patterns

### L patterns

```text
0 0001101
1 0011001
2 0010011
3 0111101
4 0100011
5 0110001
6 0101111
7 0111011
8 0110111
9 0001011
```

### G patterns

```text
0 0100111
1 0110011
2 0011011
3 0100001
4 0011101
5 0111001
6 0000101
7 0010001
8 0001001
9 0010111
```

### R patterns

```text
0 1110010
1 1100110
2 1101100
3 1000010
4 1011100
5 1001110
6 1010000
7 1000100
8 1001000
9 1110100
```

## 19. EAN left parity from first digit

The first digit is not encoded directly.
It selects the L/G parity of digits 2 through 7.

```text
first digit   parity for digits 2..7
0             LLLLLL
1             LLGLGG
2             LLGGLG
3             LLGGGL
4             LGLLGG
5             LGGLLG
6             LGGGLL
7             LGLGLG
8             LGLGGL
9             LGGLGL
```

Digits 8 through 13 always use R patterns.

## 20. EAN rendering quiet zones

For SVG rendering TableTool uses:

- 11 blank/background modules before the 95 encoded modules;
- 7 blank/background modules after the 95 encoded modules.

Thus the barcode block width before MODULE scaling is:

```text
11 + 95 + 7 = 113 modules
```

All encoded bars use the requested HEIGHT.
Guard bars are not required to extend below data bars.

---

# Part C — Code 128 B/C

## 21. Required Code 128 subset

The assignment requires an encoder using Code Sets B and C.

Code Set A, FNC symbols, SHIFT, ECI-like extensions, and GS1 semantics are out of scope.

A CODE128 cell accepts:

- 1 through 256 payload bytes;
- every byte must be printable ASCII 32 through 126 inclusive.

NULL is allowed as a cell state, but an empty non-NULL CODE128 payload is rejected.

Canonical text is exactly the original accepted ASCII payload.

## 22. Code Set B mapping

In Code Set B, printable ASCII character code `c` from 32 through 126 maps to:

```text
code value = c - 32
```

Therefore:

```text
space => 0
!     => 1
A     => 33
a     => 65
~     => 94
```

One B data codeword consumes one payload byte.

## 23. Code Set C mapping

Code Set C encodes two consecutive ASCII decimal digits as one code value 0 through 99.

Examples:

```text
"00" => 0
"05" => 5
"42" => 42
"99" => 99
```

One C data codeword consumes exactly two payload bytes.
C cannot encode an odd single digit by itself.

## 24. Start and switch values

Required values:

```text
Code C switch = 99
Code B switch = 100
Start B       = 104
Start C       = 105
Stop          = 106
```

The encoder may start in B or C.
It may switch between B and C as needed.

## 25. Optimal B/C encoding requirement

The encoder must choose a shortest valid codeword sequence for the complete payload.

The measured sequence includes:

- one start code;
- data codewords;
- B/C switch codewords.

It does not include checksum or stop when comparing candidate lengths because every complete candidate adds exactly one of each.

A valid encoding may be viewed as a segmentation into non-empty B and C runs:

- a B run contains one or more printable ASCII bytes and emits one data codeword per byte;
- a C run contains an even number of decimal digits, at least two, and emits one data codeword per digit pair;
- changing run type emits the corresponding switch codeword;
- the start code identifies the first run type.

The implementation must find a globally shortest valid segmentation.
A fixed heuristic that is known to produce a longer encoding for some input does not satisfy this requirement.

Dynamic programming is a natural solution but is not mandated.

## 26. Code 128 deterministic tie-breaking

If multiple valid encodings have the same shortest number of pre-checksum codewords, choose by this ordered rule:

1. fewer B/C switch codewords;
2. Start B over Start C;
3. lexicographically smaller sequence of numeric code values.

This tie-break makes SVG output deterministic.

## 27. Code 128 checksum

Let the chosen sequence before checksum be:

```text
start, c1, c2, ... , cn
```

where `c1..cn` include both data and switch codewords.

Compute:

```text
sum = start
    + 1*c1
    + 2*c2
    + ...
    + n*cn

checksum = sum mod 103
```

The complete symbol-value sequence is:

```text
start, c1, ..., cn, checksum, 106
```

## 28. Code 128 module patterns

For values 0 through 105, each pattern has six width digits.
The widths alternate:

```text
bar, space, bar, space, bar, space
```

and sum to 11 modules.

Stop value 106 has seven width digits and sums to 13 modules.

The following table is authoritative:

```text
0   212222
1   222122
2   222221
3   121223
4   121322
5   131222
6   122213
7   122312
8   132212
9   221213
10  221312
11  231212
12  112232
13  122132
14  122231
15  113222
16  123122
17  123221
18  223211
19  221132
20  221231
21  213212
22  223112
23  312131
24  311222
25  321122
26  321221
27  312212
28  322112
29  322211
30  212123
31  212321
32  232121
33  111323
34  131123
35  131321
36  112313
37  132113
38  132311
39  211313
40  231113
41  231311
42  112133
43  112331
44  132131
45  113123
46  113321
47  133121
48  313121
49  211331
50  231131
51  213113
52  213311
53  213131
54  311123
55  311321
56  331121
57  312113
58  312311
59  332111
60  314111
61  221411
62  431111
63  111224
64  111422
65  121124
66  121421
67  141122
68  141221
69  112214
70  112412
71  122114
72  122411
73  142112
74  142211
75  241211
76  221114
77  413111
78  241112
79  134111
80  111242
81  121142
82  121241
83  114212
84  124112
85  124211
86  411212
87  421112
88  421211
89  212141
90  214121
91  412121
92  111143
93  111341
94  131141
95  114113
96  114311
97  411113
98  411311
99  113141
100 114131
101 311141
102 411131
103 211412
104 211214
105 211232
106 2331112
```

## 29. Code 128 rendering quiet zones

For SVG rendering TableTool places:

- 10 blank/background modules before the start symbol;
- 10 blank/background modules after the stop symbol.

All bars use requested HEIGHT.

Total encoded width depends on the selected codeword sequence:

- every value 0..105 contributes 11 modules;
- stop contributes 13 modules;
- quiet zones contribute 20 modules.

---

# Part D — SVG barcode sheet

## 30. SVG requirement

BARCODE-SHEET writes a standalone UTF-8 SVG document in binary output mode with LF line endings.

No bitmap/image library is used.
The output must be readable as XML/SVG text and render in a normal SVG-capable viewer.

The root `<svg>` element must declare at least:

```text
xmlns="http://www.w3.org/2000/svg"
```

After sheet geometry is known, the root element must also express the exact integer dimensions defined below:

```text
width="W"
height="H"
viewBox="0 0 W H"
```

where `W` is the specified SVG document width and `H` is the specified sheet height, written as canonical unsigned decimal integers.
The width/height/viewBox values must agree exactly.

A fixed SVG 1.1 doctype is not required.

## 31. Block ordering

For the selected barcode column:

- scan active rows from 1 to row_count;
- skip NULL;
- render every non-NULL value once;
- preserve row order;
- duplicates remain duplicates.

Each rendered barcode is one vertical block.

## 32. Barcode geometry

For each block:

- quiet zones are included in width;
- x=0 corresponds to the left edge of the full block including quiet zone;
- bar rectangles begin after the left quiet zone;
- every black run is emitted as an SVG `<rect>` or an equivalent black path;
- white spaces need not be emitted as rectangles;
- bar y-position is the block's top y;
- bar height is exactly HEIGHT;
- horizontal module width is exactly MODULE user units.

MODULE must be from 1 through 100.
HEIGHT must be from 20 through 2000.
GAP must be from 0 through 2000.

Out-of-range parameters are operation errors.

## 33. Block width and sheet width

Compute each barcode block's width from its module count times MODULE.

The SVG document width is the maximum block width among rendered values.

If there are no non-NULL values, width is `1`.

## 34. Text labels

When `TEXT NO`:

- block vertical size is HEIGHT;
- next block starts after HEIGHT + GAP.

When `TEXT YES`:

- add a payload text label beneath the bars;
- reserve exactly 20 SVG user units for the text band;
- next block starts after HEIGHT + 20 + GAP.

The label content is canonical cell text.

The label baseline y-coordinate is exactly:

```text
block_y + HEIGHT + 15
```

Exact font family is not mandated.
The `<text>` element must use a deterministic font-size value of `14`.

## 35. XML escaping

CODE128 payload may contain characters special to XML.

At minimum, label text must escape:

```text
&  -> &amp;
<  -> &lt;
>  -> &gt;
```

Escaping `"` and `'` is also acceptable.

Unescaped payload must never be injected into XML markup or attributes.

## 36. Sheet height

If at least one barcode is rendered:

```text
height = sum(block_vertical_size) + GAP * (count - 1)
```

If zero barcodes are rendered:

```text
height = 1
```

No extra top or bottom margin is required.

## 37. Visual style boundary

SVG aesthetics are intentionally minimal.
The assignment tests barcode correctness and file generation, not graphic design.

Required:

- black bars;
- transparent or white background;
- correct bar widths/order;
- correct blank/background quiet zones;
- exact root `width`, `height`, and matching `viewBox` geometry;
- optional text according to TEXT parameter.

Gradients, colors, logos, rounded bars, and decorative backgrounds are neither required nor relevant to acceptance.

## 38. Barcode failure behavior

BARCODE-SHEET must not discover an invalid payload in a correctly typed EAN13/CODE128 column because invalid values could not have entered that type.

If internal encoding nevertheless detects an impossible state after typed-value validation, treat it as internal consistency error 8 and do not silently render a placeholder.

## 39. No external barcode generation

It is explicitly prohibited to:

- invoke an online service;
- invoke a barcode CLI;
- embed pre-rendered barcode images;
- ship only a lookup of acceptance inputs;
- call a third-party barcode library.

The submitted C source must compute check digits, codeword sequence, checksum, patterns, and SVG geometry.
