# jems vendoring

Upstream: https://github.com/rdpoor/jems
Commit: 3199b5d4ddc5eaaebb6db6bd2cd2614e3642cf49
Copyright (c) 2022 R. Dunbar Poor; MIT license retained in LICENSE and source headers.

Local changes:

- JEMS_NO_NUMBERS defaults to 1 in jems.h; conditional declarations/definitions
  omit the stdio-based double/int64 formatting APIs and their stdio/inttypes
  includes. Applications needing the upstream numeric API can define it as 0.
  Beets emits integer decimal tokens through jems_literal instead.
- Replaced sprintf-based string quoting with a fixed four-hex-digit emitter.
- jems_string and jems_bytes now decode UTF-8, emitting ASCII JSON escapes for
  non-ASCII codepoints. Supplementary Unicode emits a valid UTF-16 surrogate
  pair. Invalid UTF-8 bytes become U+FFFD; embedded NUL bytes in jems_bytes are
  escaped. No incomplete sequence reads beyond the explicit input length.
- The jems_literal byte loop uses size_t to match its length argument.

The UTF-8 change is intentional: upstream escapes individual UTF-8 bytes as
Latin-1 Unicode values, which corrupts strings containing non-ASCII characters.
No allocator or IO was added. The original stack-depth API remains unchanged;
Beets supplies six levels for a fixed maximum nesting depth of four.
