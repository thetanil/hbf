there are two broad category of use cases. prod and dev

prod should deliver pages, fragments and events to users which have a token on
your site. This is fine if it is just you. The first token is free, and it is
then the admin account. new sessions will require http auth to access admin areas.
the expected view is vertical mobile portrait view.

the dev use cases are expected to be desktop browser based, landscape
orientation but the surface is split into 3 equal column layout. the left is
further split into a stack of file selector / navigator on top, and a codemirror
text editor on the bottom. in the center column is the prod view, loaded as an
iframe, and on the right, there is a chat dialogue / feed scoller. feeds will be
defined later.

one important aspect is that the system itself is stateful for multiple clients
at the same time. muliple users, mobile and desktop, browsing the 'site' which
is a sqlite database backed stateful session for each user. only the session
cookie (or jwt-token) is stored in the browser.

## Coding Standards (C99)
Strict C99 only, zero warnings:
Flags enforced (see DOCS/coding-standards.md):
```
-std=c99 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wdouble-promotion 
-Wshadow -Wformat=2 -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes 
-Wmissing-declarations -Wcast-qual -Wwrite-strings -Wcast-align -Wundef 
-Wswitch-enum -Wswitch-default -Wbad-function-cast
```
Style: Linux kernel (tabs, 8 width). SPDX header every source file: `/* SPDX-License-Identifier: MIT */`.
No GPL dependencies; only MIT / BSD / Apache-2.0 / Public Domain.
Current third_party: CivetWeb (MIT), SQLite (PD), zlib.

Civet build flags (see `third_party/civetweb/civetweb.BUILD`):
- `-DNO_SSL` (line 16) - SSL/TLS disabled
- `-DNO_FILES` (line 18) - File serving disabled
- `-DUSE_IPV6=0` (line 19) - IPv6 disabled (IPv4 only)
- `-DUSE_WEBSOCKET` (line 20) - WebSocket enabled for future SSE/WS features

- js libs
    - htmx (every page)
    - codemirror (dev pages for phase 2 live edit)
    - mermaid (diagram pages much later, not considered in this implementation)
    - d3 (advanced pages much later, not considered in this implementation)
    

### first phase
- add http api endpoints to create read, update and delete content and view
  templates in the database
- make a home page which lazy loads a link list using htmx

### in later phases

- make a developer view which allows one to edit the content and view templates
  of that home page
- render a page 'home' in the center column as an iframe
- add a content node to the graph with the type markdown

