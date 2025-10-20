Design a generalized document–fragment graph model that can represent three
domains: modular source code systems, interactive fiction or
choose-your-own-adventure stories, and dialog trees for games. The data model
should be stored in SQLite, combining JSON document storage with graph
relationships and a hierarchical tagging system for composable documents.

There are three core entities: nodes, edges, and tags.

Nodes represent all atomic or composite entities. A node can be a function,
module, paragraph, scene, dialog node, or document. Each node stores:

a unique identifier (id)

a type (e.g., "function", "module", "fragment", "scene", "dialog", "document")

a JSON body that contains all content and metadata

optional generated fields like name or version extracted from the JSON for
indexing and querying

timestamps for creation or modification

Edges represent directed relationships between nodes. Each edge stores:

a unique identifier (id)

source node id (src)

destination node id (dst)

a relationship type (rel), describing how the two nodes connect

an optional JSON properties field (props) containing edge metadata such as
ordering, weights, or conditions

Relationship types are domain-specific but follow the same model.

Tags provide hierarchical labeling for nodes, enabling composable documents.
Each tag stores:

a tag identifier (tag)

a reference to the node it labels (node_id)

an optional parent tag, supporting nested hierarchies

ordering among sibling tags (order)

additional metadata as needed in JSON

Tags allow nodes to be grouped, reordered, and recombined dynamically by
traversing the parent/child relationships and respecting the specified order.

For modular source code systems:

Nodes include modules, functions, and files.

Edges describe inclusion, dependency, and call relationships. Examples:

"includes" — module includes a function

"calls" — one function calls another

"depends-on" — module depends on another module

Edge properties can store ordering or version information.

Tags can label functions for categories, feature sets, or module sections,
allowing easy recombination into new modules or builds.

For interactive fiction or choose-your-own-adventure stories:

Nodes include scenes, passages, and full stories.

Edges describe narrative flow or composition. Examples:

"includes" — a story includes scenes or fragments

"follows" — sequential order between scenes

"branch-to" — branching choice to another scene

"alternate-of" — variant versions of a scene or section

Edge properties can store ordering, branching conditions, or metadata for player
choices.

Tags can group scenes into chapters, story arcs, or thematic collections, with
parent/child hierarchies and ordering, enabling dynamic recomposition.

For dialog trees in games:

Nodes represent dialog lines, choices, characters, or conversation scripts.

Edges define conversational structure. Examples:

"next" — sequence of dialog lines

"branch-to" — player choice leads to another line or node

"condition" — edge exists only if a variable or flag condition is met

Edge properties can store conditions, triggers, or weights used by dialog logic
systems.

Tags can organize dialog fragments into conversations, sub-conversations, or
character-specific lines, with parent/child hierarchies and ordering.

This schema supports:

Fragment reuse (same fragment can appear in multiple documents or dialogs via
multiple edges)

Reordering and recombination (edges with order or branch conditions,
hierarchical tags with ordering)

Versioning and alternates (edges with relationships like "variant-of" or
"revised-from")

Flexible metadata (stored directly in node, edge, or tag JSON)

Querying and indexing through generated columns extracted from JSON fields

Graph traversal using recursive queries

Composable documents via tags, where a parent tag can define a document or
section and its ordered child tags define included fragments or subsections

Optionally, a full-text search virtual table can index textual content extracted
from JSON fields for efficient searching.

The result is a unified, extensible data model suitable for modular content
systems, structured storytelling, dialog authoring, and any system where
composable, reusable fragments need to be dynamically assembled into larger
documents or graphs.