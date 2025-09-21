ADR: Integration via Meta-Repository with Pinned Dependencies

Status: proposed
Date: 2025-09-20
Context:
We maintain a distributed monolith composed of many open-source repositories, each built with Bazel. These repositories evolve independently, with contributions from multiple parties. Some contributors extend the public components with private Bazel modules that wrap or depend on the mainline development. Ensuring that all components remain compatible while supporting parallel development is critical.

Decision

We will use a meta-repository (integration repository) as the single source of truth for pinned dependency versions across all component repositories.

The meta-repo will contain a lockfile (or equivalent mechanism) specifying the versions of all dependent repositories.

Continuous Integration (CI) in the meta-repo will run Bazel-based builds and integration tests across all components.

Updates to the pinned versions in the meta-repo will be automated via bots that open PRs when upstream component changes are available.

Only after CI passes in the meta-repo will versions be advanced, ensuring global consistency and integration stability.

Alternatives Considered

Independent Merge Queues per Repository

GitHub merge queues prevent broken main branches within each repo.

However, they do not coordinate dependency versioning across repos.

This can still result in global breakages when incompatible changes land in different repos.

Bot-Driven Dependency Bumps Without Central Lock

Tools like Dependabot or Renovate can propagate changes downstream automatically.

Without a central integration gate, breakages may cascade and cause churn in downstream repos.

Does not solve the "last known good state" problem across all components.

Full Monorepo

Would centralize all development and eliminate version skew.

Impractical due to size, ownership boundaries, and the open-source nature of many components.

Meta-Repository with Pinned Dependencies (Chosen)

Provides a single source of truth for compatible versions.

Supports both public open-source contributions and private extensions that wrap mainline development.

CI ensures stability before changes are promoted.

Scales to multiparty development without forcing a monorepo.

Consequences

Pros:

Ensures a globally consistent build across all components.

Provides a clear integration gate before advancing versions.

Works well in multi-party and open-source + private hybrid development.

Compatible with Bazel’s reproducibility guarantees.

Cons:

Adds an extra step before changes are fully integrated (meta-repo update).

Requires CI at the meta-repo level to be robust and performant.

Dependency updates may lag slightly behind individual repo merges.

✅ Decision: Use a meta-repository with pinned dependency versions as the integration point for all public and private components.
