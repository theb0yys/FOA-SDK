# Installed item previews

## Repair scope and ownership

The asset preview pane previously consumed only existing evidence and loose custom images. A registered game installation therefore produced zero rows. Refresh only searched for manifests and did not generate previews or reload the tree.

This Critical/Runtime change adds a read-only `unity-provider` for item template and icon discovery. Foundation supplies the saved active profile; the provider publishes local thumbnail evidence; the UI displays that snapshot. The provider does not own item authoring, catalog promotion, game installation, saves, runtime execution, or O3DE asset products.

The bounded cohort is the installed Addressables JSON catalog, `templates.items_assets_all.bundle`, and the icon bundles explicitly referenced by its item components. Native item paths, component icon addresses, catalog dependency entries, and bundle container objects form the reference chain. Unresolvable icons produce an explicit unsupported row. No filename guessing or placeholder image is used as evidence of a working preview.

## Input and output boundary

The worker receives one saved schema-1 workspace path. It resolves the active profile without changing the workspace, reads the game only, and writes beneath that profile's `ExtractedDataPath/PreviewArtifacts/NativeItems`. That destination must be in the workspace, outside the game, engine, and source checkout. Canonical path containment rejects symlink escapes. UnityPy receives bounded bundle bytes with an empty in-memory filesystem, preventing implicit dependency scans or external file access. It does not load game assemblies.

Completed generations contain a schema-1 `foa-native-item-discovery-index` and the existing `foa-thumbnail-artifact-evidence` contract, with optional `DisplayName` and `Category` presentation fields. The object-level index is distinct from the existing loose-file discovery index. Old thumbnails remain readable. Profile fields, source SHA-256 values, bundle object identities, output hashes, reader version, and dimensions bind the new artifacts. `FunctionCompleteAllowed` and every operational authority flag remain false. These are decoded 2D icons, not imported O3DE models or runtime proof. No workspace or canonical-interchange schema changes.

Each refresh uses a unique generation directory. Only a fully written manifest becomes visible; a failed/cancelled worker leaves the previous completed generation intact. A source change during extraction prevents publication. The pane selects the newest matching completed document and reloads after success. Old private generations are retained; no recursive cleanup of user data is performed.

### Item categories

The provider maps complete source folder paths and explicit item-type name tokens to readable presentation groups: Armor, Weapons, Jewelry, Gems, Ingredients, Consumables, Crafting, Books and notes, Keys, Quest items, Housing, Miscellaneous, and Developer templates. Subcategories describe useful types such as light armor, swords, potions, or alchemy ingredients. Tier numbers, content-pack folders, set names, and internal leaf-folder names do not become categories. Abstract, debug, experimental, and unused templates remain discoverable in their own group. Unknown source types remain in Miscellaneous; no item is removed by classification.

The pane offers separate category and subcategory filters with item counts. A category includes all its subcategories; changing the category resets the subcategory, while a successful reload preserves valid selections. Search applies within the selected group. Opening the viewer first after an Editor restart loads the saved workspace through Foundation's existing local-setup service; opening System Details first is unnecessary. This is display organization, not a game-runtime type or authoring-binding claim. Native references and stable identities remain unchanged, and older thumbnail documents remain readable.

## Dependency and execution contract

UnityPy 1.24.2 is pinned with binary dependencies in `Tools/item_preview_requirements.txt`. Its MIT license and dependency license metadata remain in the packaged vendor directory. Build-time pip installs into the build tree only; the application never downloads or installs packages at refresh time. UnityPy 1.25.3 was not selected because its required `tpk_ar` dependency lacked a compatible binary distribution in the inspected package index.

This is an image-only dependency subset, installed with `--no-deps` from the explicit pins. UnityPy's eagerly imported audio converter is replaced by a rejecting module inside the separate worker before initialization. FMOD, `fmod_toolkit`, and `pyfmodex` are neither packaged nor loaded; their audio APIs are outside the provider contract. No third-party distribution is edited.

The pinned O3DE Python runtime launches a separate worker through QProcess, with explicit arguments, working directory, bounded output, cancellation, and timeout. The UI thread does not parse Unity bundles. Thumbnail/source validation and snapshot loading run in a dedicated single-worker Qt thread pool with cancellation and a 30-second deadline. Superseded requests cannot publish a snapshot; widget destruction cancels and drains its owned work. The GUI only applies completed snapshots and renders visible icons. Source builds and installed packages resolve their own reader and vendor paths; system Python and developer-specific paths are not required.

The JSON catalog layout is checked against Unity's published Addressables 2.3.16 source (`ContentCatalogData.CreateLocator`); this establishes the reader layout, not a claim that the game uses that package version. Local compatibility must additionally be demonstrated by reference resolution and actual decoded images.

Sources: [UnityPy 1.24.2](https://pypi.org/project/UnityPy/1.24.2/), [UnityPy source and license](https://github.com/K0lb3/UnityPy), [Addressables catalog source distribution](https://github.com/needle-mirror/com.unity.addressables/blob/2.3.16/Runtime/ResourceLocators/ContentCatalogData.cs).

## Validation

Required proof includes malformed catalog offsets/counts, path containment, profile mismatch, source drift, stable item identity, missing icon behavior, bounded work, category precedence, and legacy thumbnail consumer compatibility. The operational lane must click Refresh in the compiled exact-pin Editor against an actual registered installation, verify nonempty item rows, select a row, inspect its displayed image, and verify responsiveness while the worker runs. It must also verify that category counts account for every item, parent and subcategory selections match their rows, and reload preserves valid filters. Synthetic fixtures alone cannot satisfy that lane.

Limits are 10,000 items, 100,000 objects per bundle, 2,048 bundles, 64 MiB per compressed bundle, 256 MiB expanded bundle data, 2 GiB aggregate source bytes, 16 megapixels per icon, 512-pixel generated thumbnails, 16 MiB per evidence document, 1 MiB worker diagnostics, and 180 seconds per refresh. The Editor smoke requires a maximum UI timer gap below three seconds while extracting. Refresh hashes sources before decoding and again before publishing; reopening uses installation binding and source size/modification metadata to reject ordinary stale caches. Explicit Refresh performs full hash verification again.

Measurements are recorded in private local evidence. Full 3D item model conversion, typed bindings, gameplay compatibility, runtime sign-off, full installer production, and release remain outside this repair.
