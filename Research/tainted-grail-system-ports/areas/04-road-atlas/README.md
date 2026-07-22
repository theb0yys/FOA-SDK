# Road Atlas

Avalon Core's Road Atlas is the source for the later Road Editor. Port order is inventory/names, exact geometry,
junction topology, anchors/destinations, static rules, region coverage, and scoped planning. Existing reviewed
packs may seed read-only project knowledge only when their source and approval state are preserved. No route
movement, spawning, scene mutation, or gameplay execution is authorized by the editor-side port.

The first compiled port is `RoadAtlasExtension`. It pins seven upstream schema/gate sources, carries typed road,
name, segment, junction, anchor, connector, geometry, connectivity, and evidence-requirement data, and produces
an order-independent canonical snapshot fingerprint. Planning approval fails closed when exact-profile binding,
geometry, topology, or required evidence is incomplete. The optional Road Atlas Tool Gem now supplies the
durable host-owned document and Editor vertical slice without copying reviewed upstream road payloads.
