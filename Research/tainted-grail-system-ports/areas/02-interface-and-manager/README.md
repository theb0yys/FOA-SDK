# Interface and Manager

Tainted Interface owns stable semantic UI/icon identifiers, curated asset descriptors, safe semantic-ID
resolution, fallback colors, and shared rendering concepts. FOA-SDK can reuse those contracts and editor theme
tokens. The pinned upstream's full asset payload is approximately 1.85 GB and has no established repository
licence, so no image payload is imported by this gate.

FoA Mod Manager owns runtime UI scope, cursor/input ownership, controller action registration, status providers,
and installed-mod presentation. Those become editor/runtime contracts and observations; the O3DE editor does not
call the live manager API.
