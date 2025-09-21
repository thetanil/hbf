# HBF - Harmonious Build Federator

hbf is a distributed monolith system that enables multiple organizations to
collaboratively develop and integrate software components while maintaining both
public and private dependencies. Built with Bazel and designed for multi-party
development, HBF provides reproducible builds, compliance metadata, and flexible
integration patterns.

## System Overview

HBF implements a **meta-repository pattern** where:
- A central integration repository coordinates dependency versions across all
  components
- Individual component repositories develop independently using Bazel modules
  (bzlmod)
- Public components remain universally accessible
- Private components are integrated through wrapper repositories without
  exposing internal details
- Single-file distributions are produced with full compliance metadata

## Architecture

### Core Principles

1. **Distributed Development, Centralized Integration**: Components develop
   independently but integrate through a central meta-repository with pinned
   dependency versions
2. **Public/Private Separation**: Public builds work without private
   dependencies; private integrations use wrapper repositories
3. **Bazel-First**: Built on Bazel with bzlmod for reproducible, hermetic builds
4. **Compliance-Ready**: Every release includes SBOMs, license metadata, and
   safety-critical documentation
5. **Multi-Party Friendly**: Supports collaboration across organizations without
   exposing private details

### Component Architecture

HBF uses a **wrapper repository pattern** to handle multi-party integration:

```bazel
# Public META-REPO MODULE.bazel
bazel_dep(name = "public_component_a", version = "1.0.0")
bazel_dep(name = "public_component_b", version = "2.0.0")

# Party B's WRAPPER MODULE.bazel
bazel_dep(name = "hbf_public", version = "1.0.0")  # Import public build
bazel_dep(name = "private_b", version = "2.0.0", dev_dependency = True)

# Party C's WRAPPER MODULE.bazel  
bazel_dep(name = "hbf_public", version = "1.0.0")  # Import public build
bazel_dep(name = "private_b", version = "2.0.0", dev_dependency = True)
bazel_dep(name = "private_c", version = "3.0.0", dev_dependency = True)
```

This pattern ensures:
- Public repository stays minimal and stable
- Private dependency details never leak to other parties
- Each party controls their own private ecosystem
- Integration churn in the public repository is minimized

## Component Repository Requirements

### Minimum Structure

```
component-repo/
├── MODULE.bazel              # Required: Bazel module definition
├── BUILD.bazel               # Required: Root build file
├── WORKSPACE.bazel           # Optional: Empty for bzlmod compatibility
├── src/                      # Component source code
├── LICENSE                   # Required: SPDX license identifier
├── README.md                 # Required: Component documentation
└── .github/workflows/        # Optional: CI/CD workflows
    └── integration-request.yml
```

### MODULE.bazel Requirements

- Semantic versioning (e.g., `version = "1.2.3"`)
- Explicit declaration of all public dependencies
- Private dependencies marked as `dev_dependency = True`
- Full bzlmod compatibility (no WORKSPACE dependencies)

### Integration Process

Components request integration builds via:

1. **Repository Dispatch Events** - Automated from component CI
2. **Workflow Dispatch** - Manual triggers via GitHub API  
3. **Bot-driven PRs** - Dependency version bumps to meta-repo

### Validation Requirements

Before acceptance, components must pass:

1. **Bazel Build Success** - Builds in isolation
2. **Compliance Check** - LICENSE files, SPDX identifiers
3. **Version Consistency** - MODULE.bazel matches git tags
4. **Integration Tests** - No breakage in meta-repo CI
5. **Security Scan** - No high-severity vulnerabilities
6. **Wrapper Compatibility** - Works with private extension pattern

## Build System

### Integration Repository (Meta-Repo)

The meta-repository serves as the single source of truth for compatible
component versions:

- Contains lockfile specifying pinned versions of all components
- Runs CI across all integrated components
- Only advances versions after successful integration testing
- Produces single-file distributions with compliance metadata

### Merge Queue Strategy

Uses **meta-repository with pinned dependencies** (see
`adr_integrate-meta-repo-pinned.md`):

- Bot-driven PRs propose dependency updates
- CI validates compatibility across all components
- Only compatible version combinations are merged
- Provides "last known good state" across the entire system

### Artifact Format

Releases follow a **Bazel package format** (see
`adr_artifact-format-metadata.md`):

- **Primary**: `.tar.gz` archive with MODULE.bazel for Bazel consumers
- **Secondary**: Optional container images for non-Bazel workflows
- **Versioning**: Semantic versioning with git tag matching
- **Metadata**: SBOM, license info, SCA reports, MISRA compliance, ISO 26262
  evidence

## GitHub Integration

### Build Triggers

Integration builds are triggered via:
- **Workflow Dispatch** - Manual triggers
- **Repository Dispatch** - Component-initiated builds
- **GitHub Checks API** - Status reporting back to components

### Multi-Organization Support

- Public builds work across organizations
- Private integrations use wrapper repositories with GitHub PATs
- Build results communicated via GitHub Checks API

## Development Environment

### DevContainer Setup

Uses `ghcr.io/eclipse-score/devcontainer:latest` providing:
- Up-to-date Git with LFS support
- Python 3 and pip3
- Standard Linux development tools
- Bazel (via eclipse-score configuration)

### Local Development

```bash
# Clone and enter devcontainer
git clone https://github.com/thetanil/hbf.git
cd hbf
# Open in VS Code with devcontainer extension

# Build all components
bazel build //...

# Run integration tests  
bazel test //...
```

## Getting Started

### For Component Developers

1. Structure your repository according to [Component
   Requirements](#component-repository-requirements)
2. Ensure your `MODULE.bazel` follows bzlmod conventions
3. Add integration workflow to request builds from HBF meta-repo
4. Submit PR to add your component to the integration lockfile

### For Private Integrations

1. Create a wrapper repository importing the public HBF build
2. Add your private dependencies as `dev_dependency = True` overrides
3. Use configuration flags to conditionally enable private features
4. Maintain your private CI/CD independent of public integration

### For Consumers

```bazel
# In your MODULE.bazel
bazel_dep(name = "hbf", version = "1.2.3")

# Or via http_archive
http_archive(
    name = "hbf",
    urls = ["https://github.com/thetanil/hbf/releases/download/v1.2.3/hbf-1.2.3.tar.gz"],
    sha256 = "...",
)
```

## Documentation

- [`qna.md`](qna.md) - Complete Q&A covering all implementation decisions
- [`spec_optional-multi-party-deps.md`](spec_optional-multi-party-deps.md) -
  Multi-party dependency specification
- [`adr_integrate-meta-repo-pinned.md`](adr_integrate-meta-repo-pinned.md) -
  Integration strategy ADR
- [`adr_artifact-format-metadata.md`](adr_artifact-format-metadata.md) -
  Artifact format specification

## Roadmap

### MVP (Public Components Only)
- Meta-repository with dependency pinning
- Basic integration CI/CD
- Public component validation
- Single-file distribution generation

### Phase 2 (Private Integration)
- Wrapper repository patterns
- Multi-party GitHub integration
- Private dependency handling
- Enhanced security scanning

### Phase 3 (Advanced Features)
- Distributed merge queue coordination
- Advanced compliance reporting
- Cross-organization access control
- Tool qualification for safety-critical applications

## Key Design Decisions

### Hard vs Soft Dependencies

HBF introduces the concept of:
- **Hard Dependencies**: In MODULE.bazel, fail the build if not present
- **Soft Dependencies**: Optional dependencies that don't break builds when
  missing

### Component Discovery

HBF does not automatically discover or register components globally. Instead:
- Components add themselves through their owning party's bazel MODULE wrapper
  configuration
- The public repo stays lean and universal
- Each party controls its own private ecosystem without exposing details

### Multi-Party Integration

The system supports complex scenarios where:
- Party A builds only public dependencies
- Party B builds with public + private_b 
- Party C builds with public + private_b + private_c

All parties can collaborate without exposing private details to others.

## License

[License details from LICENSE file]
