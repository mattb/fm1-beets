/**
 * @file jems.c
 *
 * MIT License
 *
 * Copyright (c) 2022 R. Dunbar Poor
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

// *****************************************************************************
// Includes

#include "jems.h"

#if !JEMS_NO_NUMBERS
#include <inttypes.h>
#include <stdio.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// *****************************************************************************
// Private types and definitions

// *****************************************************************************
// Private (static) storage

// *****************************************************************************
// Private (static, forward) declarations

static jems_t *push_level(jems_t *jems, bool is_object);
static jems_t *pop_level(jems_t *jems);
static jems_t *emit_char(jems_t *jems, char ch);
static jems_t *emit_quoted_byte(jems_t *jems, uint8_t byte);
static jems_t *emit_string(jems_t *jems, const char *s);
static jems_t *emit_quoted_string(jems_t *jems, const char *s);
static jems_t *emit_quoted_bytes(jems_t *jems, const uint8_t *bytes,
                                 size_t len);
static jems_t *commify(jems_t *jems);
static jems_level_t *level_ref(jems_t *jems);

// *****************************************************************************
// Public code

jems_t *jems_init(jems_t *jems, jems_level_t *levels, size_t max_level,
                  jems_writer_fn writer, uintptr_t arg) {
  jems->levels = levels;
  jems->max_level = max_level;
  jems->writer = writer;
  jems->arg = arg;
  return jems_reset(jems);
}

jems_t *jems_reset(jems_t *jems) {
  jems->curr_level = 0;
  level_ref(jems)->item_count = 0;
  level_ref(jems)->is_object = false;
  return jems;
}

jems_t *jems_object_open(jems_t *jems) {
  commify(jems);
  emit_char(jems, '{');
  return push_level(jems, true);
}

jems_t *jems_object_close(jems_t *jems) {
  emit_char(jems, '}');
  return pop_level(jems);
}

jems_t *jems_array_open(jems_t *jems) {
  commify(jems);
  emit_char(jems, '[');
  return push_level(jems, false);
}

jems_t *jems_array_close(jems_t *jems) {
  emit_char(jems, ']');
  return pop_level(jems);
}

#if !JEMS_NO_NUMBERS
jems_t *jems_number(jems_t *jems, double value) {
  char buf[22];
  int64_t i = value;
  if ((double)i == value) {
    // if number can be represented exactly as an int, print as int
    snprintf(buf, sizeof(buf), "%" PRId64, i);
  } else {
    snprintf(buf, sizeof(buf), "%lf", value);
  }
  commify(jems);
  return emit_string(jems, buf);
}
#endif

#if !JEMS_NO_NUMBERS
jems_t *jems_integer(jems_t *jems, int64_t value) {
  char buf[22]; // 20 digits, 1 sign, 1 null
  snprintf(buf, sizeof(buf), "%" PRId64, value);
  commify(jems);
  return emit_string(jems, buf);
}
#endif

jems_t *jems_string(jems_t *jems, const char *string) {
  commify(jems);
  emit_char(jems, '"');
  emit_quoted_string(jems, string);
  return emit_char(jems, '"');
}

jems_t *jems_bytes(jems_t *jems, const uint8_t *bytes, size_t length) {
  commify(jems);
  emit_char(jems, '"');
  emit_quoted_bytes(jems, bytes, length);
  return emit_char(jems, '"');
}

jems_t *jems_bool(jems_t *jems, bool boolean) {
  commify(jems);
  return emit_string(jems, boolean ? "true" : "false");
}

jems_t *jems_true(jems_t *jems) {
  commify(jems);
  return emit_string(jems, "true");
}

jems_t *jems_false(jems_t *jems) {
  commify(jems);
  return emit_string(jems, "false");
}

jems_t *jems_null(jems_t *jems) {
  commify(jems);
  return emit_string(jems, "null");
}

jems_t *jems_literal(jems_t *jems, const char *literal, size_t n_bytes) {
  commify(jems);
  for (size_t i = 0; i < n_bytes; i++) {
    const uint8_t b = literal[i];
    emit_char(jems, b);
  }
  return jems;
}

// ***************
// key:value pairs

jems_t *jems_key_object_open(jems_t *jems, const char *key) {
  return jems_object_open(jems_string(jems, key));
}

jems_t *jems_key_array_open(jems_t *jems, const char *key) {
  return jems_array_open(jems_string(jems, key));
}

#if !JEMS_NO_NUMBERS
jems_t *jems_key_number(jems_t *jems, const char *key, double value) {
  return jems_number(jems_string(jems, key), value);
}
#endif

#if !JEMS_NO_NUMBERS
jems_t *jems_key_integer(jems_t *jems, const char *key, int64_t value) {
  return jems_integer(jems_string(jems, key), value);
}
#endif

jems_t *jems_key_string(jems_t *jems, const char *key, const char *string) {
  return jems_string(jems_string(jems, key), string);
}

jems_t *jems_key_bytes(jems_t *jems, const char *key, const uint8_t *bytes,
                       size_t length) {
  return jems_bytes(jems_string(jems, key), bytes, length);
}

jems_t *jems_key_bool(jems_t *jems, const char *key, bool boolean) {
  return jems_bool(jems_string(jems, key), boolean);
}

jems_t *jems_key_true(jems_t *jems, const char *key) {
  return jems_true(jems_string(jems, key));
}

jems_t *jems_key_false(jems_t *jems, const char *key) {
  return jems_false(jems_string(jems, key));
}

jems_t *jems_key_null(jems_t *jems, const char *key) {
  return jems_null(jems_string(jems, key));
}

jems_t *jems_key_literal(jems_t *jems, const char *key, const char *literal,
                         size_t n_bytes) {
  return jems_literal(jems_string(jems, key), literal, n_bytes);
}

size_t jems_curr_level(jems_t *jems) { return jems->curr_level; }

size_t jems_item_count(jems_t *jems) { return level_ref(jems)->item_count; }

// *****************************************************************************
// Private (static) code

static jems_t *push_level(jems_t *jems, bool is_object) {
  if (jems->curr_level < jems->max_level - 1) {
    jems->curr_level += 1;
    level_ref(jems)->item_count = 0;
    level_ref(jems)->is_object = is_object;
  }
  return jems;
}

static jems_t *pop_level(jems_t *jems) {
  if (jems->curr_level > 0) {
    jems->curr_level -= 1;
  }
  return jems;
}

static jems_t *emit_char(jems_t *jems, char ch) {
  jems->writer(ch, jems->arg);
  return jems;
}

/* FM-1 patch: emit ASCII-only UTF-16 escapes without stdio. */
static void emit_u16(jems_t *jems, uint32_t cp) {
  static const char hex[] = "0123456789abcdef";
  int shift;
  emit_char(jems, '\\'); emit_char(jems, 'u');
  for (shift = 12; shift >= 0; shift -= 4) emit_char(jems, hex[(cp >> shift) & 15u]);
}
static jems_t *emit_quoted_byte(jems_t *jems, uint8_t byte) {
  if (byte < 0x20 || byte >= 127) emit_u16(jems, byte);
  else {
    if (byte == '\\' || byte == '"') emit_char(jems, '\\');
    emit_char(jems, (char)byte);
  }
  return jems;
}
static jems_t *emit_string(jems_t *jems, const char *s) {
  while (*s) emit_char(jems, *s++);
  return jems;
}
static jems_t *emit_quoted_string(jems_t *jems, const char *s) {
  size_t len = 0;
  while (s[len]) len++;
  return emit_quoted_bytes(jems, (const uint8_t *)s, len);
}
/* Decode valid UTF-8 before escaping. Invalid sequences become U+FFFD, one
 * offending byte at a time; truncated input never reads beyond length. */
static jems_t *emit_quoted_bytes(jems_t *jems, const uint8_t *bytes, size_t len) {
  size_t i = 0;
  while (i < len) {
    uint32_t cp, minimum;
    size_t count, k;
    uint8_t c = bytes[i];
    if (c < 128) { emit_quoted_byte(jems, c); i++; continue; }
    if (c >= 0xc2 && c <= 0xdf) { cp = c & 31u; count = 2; minimum = 0x80; }
    else if (c >= 0xe0 && c <= 0xef) { cp = c & 15u; count = 3; minimum = 0x800; }
    else if (c >= 0xf0 && c <= 0xf4) { cp = c & 7u; count = 4; minimum = 65536u; }
    else { emit_u16(jems, 0xfffd); i++; continue; }
    if (len - i < count) { emit_u16(jems, 0xfffd); i++; continue; }
    for (k = 1; k < count; k++) {
      if ((bytes[i+k] & 0xc0u) != 0x80u) break;
      cp = (cp << 6) | (bytes[i+k] & 63u);
    }
    if (k != count || cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
      emit_u16(jems, 0xfffd); i++; continue;
    }
    i += count;
    if (cp <= 0xffff) emit_u16(jems, cp);
    else { cp -= 65536u; emit_u16(jems, 0xd800u | (cp >> 10)); emit_u16(jems, 0xdc00u | (cp & 1023u)); }
  }
  return jems;
}

static jems_t *commify(jems_t *jems) {
  jems_level_t *level = level_ref(jems);
  size_t count = level->item_count;
  if (level->is_object) {
    // within { ... }:
    // the 0th item has no prefix
    // odd items are prefixed with a ':'
    // even items are prefixed with a ','
    if (count > 0) {
      emit_char(jems, (count & 1) ? ':' : ',');
    }
  } else {
    // for other lists:
    // the 0th item has no prefix
    // all other items are prefixed with ','
    if (count > 0) {
      emit_char(jems, ',');
    }
  }
  level->item_count += 1;
  return jems;
}

static jems_level_t *level_ref(jems_t *jems) {
  return &jems->levels[jems->curr_level];
}

// *****************************************************************************
// End of file
