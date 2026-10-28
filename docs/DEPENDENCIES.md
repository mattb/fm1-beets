# Referencing fm1-base

Git tracks `fm1-base` as a submodule pinned to a commit. The gitlink has mode
`160000`, and `.gitmodules` holds the upstream URL. Setup checks out that revision
in a separate directory. Set `FM1_BASE` to use a shared or experimental checkout.

## Default setup

```sh
tools/setup-local.sh
tools/build-local.sh --all
```

Setup and build initialize the pinned submodule and its nested BSP when missing.
A normal clone works; `git clone --recurse-submodules` is optional. An existing
checkout is not reset on every build. To deliberately restore recorded dependency
revisions, inspect local changes first, then use Git's submodule update workflow.

`patches/fm1-base-external-build.patch` makes Docker mount the app and base
directories separately, so they can be in unrelated locations. The separate
`patches/fm1-base-telemetry.patch` supplies the nonblocking USB
API needed by Beets. It is applied idempotently to the default submodule, so Git
reports that submodule as modified. The tracked patch reproduces those edits after a fresh clone. All other platform source stays in
the upstream repository.

## Shared checkout: FM1_BASE

For multiple firmware projects, use one external checkout or a dedicated Git
worktree. Paths may be absolute or relative to the directory where you invoke
the wrapper; spaces are supported.

```sh
# Initialize the dependency's own nested BSP first:
git -C /path/to/fm1-base submodule update --init --recursive
# Explicit, idempotent modification of the selected checkout:
tools/patch-base.sh /path/to/fm1-base

export FM1_BASE=/path/to/fm1-base
tools/setup-local.sh
tools/build-local.sh --all
tests/run-tests.sh --no-build
```

With `FM1_BASE` set, wrappers neither fetch/reset that checkout nor apply patches
to it. They validate its required files and the expected patches, then use it for
all three build targets and test include paths. Setup skips the local submodule
entirely. An invalid override stops the build.

The external checkout's revision is your responsibility. For a reproducible
release, use the default pinned dependency, or record the external commit and
patches with the build evidence. The current patch is validated against the
recorded base; a future upstream implementation may require updating/removing the
patch and its check, even if it provides equivalent APIs.

Resolve the selected checkout for other platform tools:

```sh
BASE=$(tools/prepare-base.sh)
.venv/bin/python "$BASE/tools/fm1_install.py" --info
sh "$BASE/tools/get_toolchain.sh"
```

Build artifacts stay in `app/build/` when the dependency location changes. Beets
source, assets, settings and TouchOSC files stay in this repository. Rebuild each
target after switching dependencies before running compatibility tests.

## Updating the pin

Review the selected upstream commit and local submodule changes before switching
revisions. Rebase the telemetry patch if necessary; build firmware/native/WASM,
run the tests, and test USB on the device. Commit the new gitlink and any patch
changes together. Keep commits referenced by the gitlink available at the URL in
`.gitmodules`; do not point collaborators at a local-only commit.
