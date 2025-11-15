# Hypermedia Fragments: Implementation Design

This document outlines a practical implementation design for a hypermedia system built on SQLite. It merges the concepts of a property graph for storing content fragments with a lightweight, C99-friendly templating mechanism. The goal is to create a robust, high-performance system suitable for a small, statically-linked C web server, serving content for both full pages and HTMX-powered partial updates.

---

## 1. Core Principles

*   **Fragments as Graph Nodes**: Content is stored as "fragments" (e.g., Markdown, HTML), which are treated as nodes in a directed graph.
*   **Lightweight C99 Templating**: Rendering is performed via a simple, fast string-replacement function in C. It avoids complex parsing, ASTs, and external scripting, making it ideal for a minimal binary.
*   **SQLite as Single Source of Truth**: The database stores content, graph relationships, and presentation templates.
*   **Designed for Embedded Systems**: The entire approach is optimized for a small memory footprint and high performance within a self-contained C application.

---

## 2. Database Schema

The schema combines the graph structure with a new table for managing presentation templates.

### `nodes` Table
Stores the core content fragments. Each node has a `view_type` that links it to a rendering template.

```sql
CREATE TABLE nodes (
    id INTEGER PRIMARY KEY,
    view_type TEXT NOT NULL DEFAULT 'default',
    content TEXT NOT NULL,
    properties JSON,
    version INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    -- Optional: cache for rendered HTML
    html_cache TEXT,
    FOREIGN KEY(view_type) REFERENCES view_type_templates(view_type)
);
```

### `view_type_templates` Table
Stores the presentation templates for different types of views. This is the core of the C99-friendly templating engine.

```sql
CREATE TABLE view_type_templates (
    view_type TEXT PRIMARY KEY,
    template TEXT NOT NULL
);
```

**Example Template:**
For a `view_type` of `'card'`, the template might be:
```html
<div class="card fragment-{{view_type}}" id="frag-{{id}}">
    <div class="card-content">
        {{content}}
    </div>
</div>
```

### `edges` Table
Defines the relationships between nodes, forming the hypermedia graph.

```sql
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
```

### `closure` Table (Optional)
For performance, a closure table can be maintained with triggers to pre-compute all paths in the graph, enabling fast queries for descendants and backlinks.

```sql
CREATE TABLE closure (
    ancestor INTEGER NOT NULL,
    descendant INTEGER NOT NULL,
    depth INTEGER NOT NULL,
    PRIMARY KEY (ancestor, descendant)
);
```

---

## 3. Rendering Pipeline

Rendering a fragment is a multi-step process that combines content processing with template substitution.

1.  **Fetch Data**: Retrieve the node (`content`, `view_type`, `id`, etc.) from the `nodes` table.
2.  **Process Content**: If the `content` is in a format like Markdown, process it into HTML. This can be done with a small C library like `md4c`.
3.  **Fetch Template**: Retrieve the corresponding `template` from `view_type_templates` using the node's `view_type`.
4.  **Render**: Use the tiny C99 `render_template` function to substitute placeholders in the template with the processed content and other node data.

### Tiny Template Renderer (C99)

This function performs simple string scanning and replacement, avoiding the overhead of a full templating engine.

```c
/* A simple key-value pair for placeholder substitution. */
struct placeholder {
    const char *key;
    const char *value;
};

/*
 * Renders a template by replacing {{key}} placeholders with values.
 *
 * This function is C99-compatible, performs no complex parsing, and is
 * designed for performance in an embedded environment. The caller is
 * responsible for freeing the returned string.
 */
char* render_template(const char *tmpl,
                      const struct placeholder *repls,
                      size_t repl_count)
{
    // (Implementation as provided in the design document)
    // ...
    return out; // The final rendered string.
}
```

### Example Rendering Flow

```c
// 1. Assume we have fetched a node's data.
const char *node_id_str = "123";
const char *node_view_type = "card";
const char *markdown_content = "This is the **content**.";

// 2. Process the content.
char *rendered_html = markdown_to_html(markdown_content);

// 3. Fetch the template for the 'card' view_type.
const char *template = fetch_template_from_db("card");
// e.g., "<div id='frag-{{id}}'>{{content}}</div>"

// 4. Prepare placeholders and render.
struct placeholder repls[3] = {
    { "id", node_id_str },
    { "view_type", node_view_type },
    { "content", rendered_html }
};

char *final_html = render_template(template, repls, 3);

// final_html is now: "<div id='frag-123'>This is the <strong>content</strong>.</div>"

free(rendered_html);
free(final_html);
```

---

## 4. Page and Fragment Assembly

This system supports both full-page builds and individual fragment requests (for HTMX).

### Fragment Rendering (HTMX)

An endpoint like `GET /fragment/:id` will execute the rendering pipeline for a single node and return the resulting HTML. This is ideal for HTMX-driven partial page updates.

### Full Page Assembly

A "full page" is just a special template (e.g., with `view_type = 'page'`) that contains a `{{content}}` placeholder for the main body.

```html
<!-- Template for view_type = 'page' -->
<html>
  <head><title>My Page</title></head>
  <body>
    <main>
      {{content}}
    </main>
  </body>
</html>
```

To assemble a page (e.g., at `GET /assemble/:id`):
1.  Use a recursive SQL query to traverse the graph from a starting node (`:id`), collecting all descendant fragments.
2.  For each fragment, run the rendering pipeline to generate its HTML.
3.  Concatenate the HTML of all fragments into a single string.
4.  Render the final "page" template, using the concatenated string as the value for the `{{content}}` placeholder.

---

## 5. API Endpoints

The API remains minimal and maps directly to database operations, now with rendering integrated.

*   `GET /fragment/:id`: Renders and returns a single fragment as HTML.
*   `GET /assemble/:id`: Traverses the graph from `:id`, renders all descendant fragments, and assembles them into a full HTML page.
*   `POST /fragment`: Creates a new node.
*   `PUT /fragment/:id`: Updates an existing node.
*   `POST /link`: Creates an edge between two nodes.
*   `DELETE /link/:id`: Deletes an edge.

This design provides a complete, efficient, and maintainable hypermedia system that is perfectly suited for the constraints of a tiny C binary.
