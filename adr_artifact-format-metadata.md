ADR: Build Artifacts & Releases Format and Metadata
Context

The system must produce a “single file distribution” of components that can be consumed by downstream builds. The distribution must support Bazel-based consumers as a first-class use case, while also meeting compliance, safety, and interoperability requirements.

The following questions must be addressed:

Should the distribution be a binary, archive, or container image?

How should releases be tagged and versioned?

What metadata must accompany each release?

Downstream builds are expected to rely on Bazel’s module system (MODULE.bazel with bazel_dep or http_archive) for reproducibility and dependency management. In parallel, safety-critical projects require compliance evidence such as SBOMs, static analysis reports, and tool qualification documentation (ISO 26262).

Decision

Primary Distribution Format

The single file distribution will be a Bazel package published as an archive file (.tar.gz or .zip).

The package must contain:

A MODULE.bazel or equivalent metadata for Bazel resolution.

All source, generated files, and BUILD files needed for downstream consumption.

Downstream consumers will integrate this package using bazel_dep (preferred) or http_archive.

Secondary Formats (Optional)

For non-Bazel consumers, optional container images or standalone archives may be produced.

These formats are secondary and do not replace Bazel package distribution.

Release Tagging and Versioning

Releases will follow semantic versioning (semver).

Release tags must match the version declared in the Bazel module (for example, v1.2.3).

Downstream users can pin exact versions in their MODULE.bazel or via archive URLs with checksums.

Release Metadata
Each release must include compliance and reproducibility metadata:

SBOM (Software Bill of Materials): generated via Syft, Fossology, or equivalent tools.

License metadata: SPDX identifiers and bundled third-party license texts.

SCA reports: software composition analysis results.

MISRA compliance report: where applicable for safety-critical code.

ISO 26262 tool qualification evidence: qualification kits or tool confidence arguments for toolchain components.

Checksums and signatures: cryptographic integrity checks (e.g., SHA256) and optional signing.

Rationale

Bazel package as primary format: ensures seamless downstream consumption in Bazel builds, with reproducibility guarantees and minimal integration overhead.

Archive as single file: enables simple distribution, storage, and integrity validation.

Semantic versioning: provides predictable upgrade paths and compatibility signaling.

Metadata inclusion: ensures compliance with open source license requirements, security scanning practices, and functional safety standards (ISO 26262).

Wrapper formats optional: container images or plain archives may support non-Bazel consumers but should not dictate primary workflows.

Consequences

Public consumers can reliably fetch and build using only the Bazel package.

Safety-critical projects can satisfy compliance and tool qualification needs from bundled metadata.

Releasing requires coordination between build outputs (archive) and compliance tooling (SBOM, SCA, MISRA, ISO 26262).

Release automation must handle both packaging and metadata generation as part of CI/CD.
