# Practical Guide: SQLite Hypermedia Fragment Graph System

This guide outlines a practical approach for building a web-based hypermedia fragment system using SQLite as a property graph store, suitable for embedding in a C99 server (CivetWeb) with minimal HTTP API and htmx for frontend fragment assembly.

## Core Concepts
- **Nodes**: Each fragment (HTML, text, etc.) is a node in the graph.
- **Edges**: Directed links between nodes, with properties (selectors, roles, etc.) stored as JSON.
- **Selectors/Anchors**: Use W3C Web Annotation model for fragment selectors, stored in edge JSON attributes.
- **Versioning**: Support immutable snapshots via version columns or content-addressed IDs (Merkle DAG).
- **Closure Table**: Optionally precompute transitive closure for fast traversal and backlinks.
- **Full-Text Search**: Use FTS5 for querying fragment content.
- **Hybrid Storage**: The schema intentionally separates graph topology (nodes, edges) from rich attributes (JSON properties). This allows the relational engine to optimize traversals while providing flexible key-value storage for metadata, a pattern validated for high performance.

## Minimal HTTP API (for htmx/web clients)
- `GET /fragment/:id` — Read fragment by node ID
- `POST /fragment` — Create new fragment (HTML/text)
- `PUT /fragment/:id` — Update fragment (new version)
- `DELETE /fragment/:id` — Delete fragment
- `GET /links/:id` — Get outgoing/incoming edges for a node
- `POST /link` — Create edge (with selectors/roles)
- `DELETE /link/:id` — Remove edge
- `GET /assemble/:id` — Recursively assemble hypermedia by following edges (optionally using closure table)

## SQLite Schema Example
```sql
CREATE TABLE nodes (
	id INTEGER PRIMARY KEY,
	content TEXT NOT NULL,
	properties JSON,
	version INTEGER DEFAULT 1,
	created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE edges (
	id INTEGER PRIMARY KEY,
	from_id INTEGER NOT NULL,
	to_id INTEGER NOT NULL,
	relation TEXT,
	selector JSON,
	created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
	FOREIGN KEY(from_id) REFERENCES nodes(id),
	FOREIGN KEY(to_id) REFERENCES nodes(id)
);

-- Optional closure table for fast transitive queries
CREATE TABLE closure (
	ancestor INTEGER NOT NULL,
	descendant INTEGER NOT NULL,
	depth INTEGER NOT NULL,
	PRIMARY KEY (ancestor, descendant),
	FOREIGN KEY(ancestor) REFERENCES nodes(id),
	FOREIGN KEY(descendant) REFERENCES nodes(id)
);
```

## Closure Table Maintenance (SQL triggers)
```sql
-- On edge insert, update closure table
CREATE TRIGGER edge_insert_closure AFTER INSERT ON edges BEGIN
	INSERT INTO closure (ancestor, descendant, depth)
	VALUES (NEW.from_id, NEW.to_id, 1);
	-- Insert transitive closure rows
	INSERT INTO closure (ancestor, descendant, depth)
	SELECT c.ancestor, NEW.to_id, c.depth + 1
	FROM closure c WHERE c.descendant = NEW.from_id;
END;

-- On edge delete, remove closure rows
CREATE TRIGGER edge_delete_closure AFTER DELETE ON edges BEGIN
	DELETE FROM closure WHERE ancestor = OLD.from_id AND descendant = OLD.to_id;
	-- Optionally, remove transitive closure rows (requires more logic)
END;
```

## Sample Queries
- **Assemble hypermedia (recursive):**
```sql
WITH RECURSIVE fragments(id, content, depth) AS (
	SELECT n.id, n.content, 0 FROM nodes n WHERE n.id = ?
	UNION ALL
	SELECT n.id, n.content, f.depth + 1
	FROM edges e JOIN nodes n ON n.id = e.to_id
	JOIN fragments f ON f.id = e.from_id
)
SELECT * FROM fragments;
```
- **Backlinks:**
```sql
SELECT from_id FROM edges WHERE to_id = ?;
-- Or, with closure table:
SELECT ancestor FROM closure WHERE descendant = ?;
```
- **Cycle detection:**
```sql
SELECT 1 FROM closure WHERE ancestor = descendant;
```

## Implementation Notes
- Use SQLite JSON1 for selectors/roles; add indexes for performance.
- FTS5 enables full-text search on fragments.
- For UDFs, implement custom SQLite C callbacks as needed (e.g., HTML merge, selector logic).
- HTTP API should be minimal, stateless, and map directly to CRUD operations above.
- htmx can request fragments and assemble them client-side, or use `/assemble/:id` for server-side merge.
- Versioning: either increment `version` on update, or use content-addressed IDs for immutable fragments.
- Handle cycles robustly in recursive queries (track visited nodes).

## References
- Google Research: Property Graphs in RDBMS
- Percona: Closure Table Engineering
- W3C Web Annotation Model
- arXiv: Content-Addressed Graphs
- ResearchGate: Transclusion & Cycle Handling
- ePrints Soton: Open Hypermedia Systems
