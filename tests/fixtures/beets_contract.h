/* SPDX-License-Identifier: MIT
 * Literal boundary decisions derived from pinned upstream Lua; see README.md.
 * Row zero is the initial downbeat; row eight is the following downbeat. */
static const struct {
    const char *name;
    int skip, repeat, first, last, auto_next;
    unsigned mask;
    int slices[10], loops[10];
} contracts[] = {
    {"natural", 0,0,0,7,0,255, {0,1,2,3,4,5,6,7,0,1}, {0}},
    {"skip", 100,0,0,7,0,255, {0,2,4,6,0,2,4,6,0,2}, {0}},
    {"repeat", 0,100,0,7,0,255, {0,0,0,0,0,0,0,0,0,0}, {0}},
    {"skip then repeat", 100,100,0,7,0,255, {0,1,2,3,4,5,6,7,0,1}, {0}},
    {"sparse range and bar anchor", 0,0,2,6,0,84, {2,4,6,2,4,6,2,4,2,4}, {0}},
    {"auto-next wraps before bar anchor", 0,0,0,1,1,3,
        {0,1,0,1,0,1,0,1,0,1}, {0,0,1,1,2,2,3,3,4,4}},
};
