# Offline editor distribution

Vendored from the official npm package `vditor@3.11.2`:
https://registry.npmjs.org/vditor/-/vditor-3.11.2.tgz

Archive SHA-256: `cb3a10ee9dfbe67fa780e6f63eb097397d07290340e29468b94c2ee139f330dd`.

Only the editor runtime, Lute, Simplified Chinese translations, Ant icons,
highlight.js, GitHub highlighting theme and light content theme are retained.
Source files, other translations/themes, emoji images and optional diagram/math
renderers are omitted. Runtime files are unmodified. TalkMusic integration and
styling are in `editor/`, outside the third-party code.

Vditor is MIT licensed; bundled highlight.js is BSD licensed; Lute and
diff-match-patch licenses are included separately. CMake embeds these runtime
files in Qt resources and installs the license files. No npm or network is
required when building or running TalkMusic.
