# Vendored third-party code

Everything under `gui/vendor/` is third-party. Nothing else in this repository is:
the upper-case files in the root are the 1995 Borland sources, and `host/`, `cli/`,
`tests/` and `gui/` (outside this directory) are the 2026 port.

It is vendored rather than fetched because that is the property that made the 1995
sources readable in the first place. `BORG.EXE` still exists, `BWCC.DLL` still exists,
and `BORG.IDE` still points at `C:\BC4\INCLUDE` — which is exactly why the build
instructions in `BORG.IDE` are useless now. A checked-in copy has no tag to go stale,
no host to go offline, and no `submodule update` step to forget.

These files are **not** to be edited. If one ever needs patching, patch it in a
separate file under `gui/` and say so here.

`.gitattributes` marks this directory `-text`, so the bytes are stored exactly as
upstream shipped them and `shasum` below stays verifiable.

---

## Dear ImGui

| | |
|---|---|
| Upstream | https://github.com/ocornut/imgui |
| Branch   | **`docking`** — not `master`, see below |
| Version  | **v1.92.9b-docking** |
| Commit   | `b48d1afbe8ee8b238e2961dc363a949dd7304e23` |
| License  | MIT — `imgui/LICENSE.txt` |
| Retrieved | 11 Sep 2026 |

### Why the `docking` branch and not `master`

The layout needs `ImGuiConfigFlags_DockingEnable` and the `DockBuilder*` API, and
**neither exists on `master`.** Docking has lived on its own long-running branch
for years; upstream rebases it onto each release and tags it `vX.Y.Z-docking`.
Pinning a plain `vX.Y.Z` tag gets a tree with no `DockBuilderSplitNode`, and the
only symptom is a compile error — which is how this was caught here.

So the pin must carry the `-docking` suffix. Check after any update:

    grep -c DockBuilderSplitNode gui/vendor/imgui/imgui_internal.h    # must be ≥ 1

The one thing given up by using this branch with the SDL_Renderer backend is
multi-viewports — dragging a panel out into its own OS window. Docking inside the
main window, which is all the 1995 layout needs, works.

### Files taken

Core, all of it that is required to build:

    imconfig.h  imgui.h  imgui_internal.h
    imgui.cpp  imgui_draw.cpp  imgui_tables.cpp  imgui_widgets.cpp
    imstb_rectpack.h  imstb_textedit.h  imstb_truetype.h

Backends, two of the thirty-odd upstream ships:

    backends/imgui_impl_sdl3.{h,cpp}           platform: window, input, events
    backends/imgui_impl_sdlrenderer3.{h,cpp}   renderer: draws through SDL_Renderer

`imgui_impl_sdlrenderer3` rather than `imgui_impl_metal` deliberately. SDL3's
renderer already selects Metal on macOS, so this gets Metal without a line of
Objective-C, and the same two files build on Windows (Direct3D) and Linux (OpenGL)
if this is ever wanted there. The cost is that multi-viewports — dragging a panel
outside the OS window — are not supported by this backend. Docking inside the window
is, which is what the layout needs.

### Files deliberately NOT taken

    imgui_demo.cpp      ~600 KB of widget gallery. Useful while learning the
                        library, dead weight in a build. Fetch it from upstream
                        at the pinned commit if it is ever wanted.
    backends/*          the other ~28 backends (GLFW, Win32, DX9-12, OpenGL2/3,
                        Vulkan, WGPU, Allegro, Android, ...)
    examples/           per-backend sample apps
    docs/  misc/        documentation and the freetype/stdlib helpers

## stb_image_write

| | |
|---|---|
| Upstream | https://github.com/nothings/stb |
| Version  | **v1.16** (the version string inside the header) |
| Commit   | `2c980bb59875b0d32144a71867fbdebb2f77cd20` |
| License  | public domain or MIT, at your option — `stb/LICENSE.txt` |
| Retrieved | 11 Sep 2026 |

One header, used for exactly one thing: writing the field panel out as a PNG
(`gui/export.cpp`). This replaces 1995's `Save Meta File` and `makeBitmap()`, which
wrote a Windows Metafile and a `BITMAPINFO`-format `.BMP` through GDI and cannot be
ported. SDL3 core has `SDL_SaveBMP` but no PNG writer, and SDL3_image is a separate
package this build otherwise does not need.

Only `STB_IMAGE_WRITE_IMPLEMENTATION` + `stbi_write_png` are compiled; the JPG, BMP,
TGA and HDR writers are disabled by `STBI_WRITE_NO_STDIO`-adjacent defines in
`gui/export.cpp`.

---

## Verifying these bytes

    cd gui/vendor && shasum -a 256 -c SHA256SUMS

`SHA256SUMS` was generated from the files as copied. To check them against upstream
instead:

    git clone https://github.com/ocornut/imgui /tmp/imgui-check
    git -C /tmp/imgui-check checkout 01380c579715e62fb9a8d6ec0502c4ea83bfde6e
    diff -r --brief /tmp/imgui-check/backends gui/vendor/imgui/backends
