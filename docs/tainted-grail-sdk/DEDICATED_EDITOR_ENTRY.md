# Dedicated Editor Entry Architecture

Developer Preview 0 uses the repository-owned `TaintedGrailModdingEditor`
project as its product host.

The project:

- is registered by `engine.json`;
- enables `TaintedGrailModdingSDK` exactly once;
- owns its O3DE preview PNG and Windows shortcut ICO;
- is independent from the upstream `AutomatedTesting` project;
- is selected by `developer_preview_open.py`;
- is selected by the generated `Tainted Grail Modding Editor.lnk`.

The `.lnk` is generated after build because it contains machine-local absolute
paths to the source-built `Editor.exe` and repository checkout. No `.lnk` or
private absolute path is committed.

`developer_preview_entry.py` is the supported clickable-entry command. It uses
the lower-level shortcut generator, verifies the `.lnk` hash manifest, and then
inspects the actual Windows shortcut through `WScript.Shell`. Target, project
arguments, working directory, icon, and description must all match.

`--replace` first verifies the existing generated shortcut. An unrelated,
modified, or incomplete `.lnk` is never removed automatically.

The entry tooling does not install software, create a desktop entry silently,
launch FoA, or expose arbitrary Editor arguments.
