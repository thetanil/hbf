**Specification: Optional Multi-Party Dependencies in Bazel Using Wrapper Repositories**

---

### Objective

Define a strategy that allows multiple parties to share a common public Bazel repository while enabling each party to extend the build with their own private dependencies. The solution must ensure that:

* Public builds remain stable and independent of private integrations.
* Internal development details of Party B and Party C are not exposed to the public or to Party A.
* Integration churn in the public repository is minimized.
* Each party retains flexibility in how they bring in private dependencies.

---

### Context

We consider a system with three parties:

* Party A: builds only public dependencies.
* Party B: builds with public dependencies plus a private dependency (private\_b).
* Party C: builds with public dependencies plus both private\_b and another private dependency (private\_c).

The public repository must be consumable by all parties, including those without access to private dependencies. Therefore, private dependencies must be optional rather than mandatory.

---

### Decision

The solution is to use a **wrapper repository pattern**.

1. The public repository declares only public dependencies. Private dependencies are referenced as optional development-only entries (dev\_dependencies) so that Bazel does not fail when they are absent. No internal overrides for private dependencies are checked into the public repository.

2. Each party maintains its own wrapper repository. A wrapper repository imports the public repository as a dependency and applies overrides for private dependencies. This allows Party B and Party C to integrate their private repositories without requiring changes in the public repository.

3. Build rules that depend on private dependencies are conditionally activated. This is controlled via configuration flags (for example, define values) so that only the parties with the appropriate overrides and flags enabled will attempt to consume those dependencies. Parties without access simply do not activate those parts of the build.

---

### Reasoning and Implications

1. **Why wrapper repositories rather than per-party override files in the public repository?**
   If per-party overrides were checked into the public repository, details of private dependencies would become visible to all parties, including Party A and any external users. This would leak sensitive information such as the existence, naming, and structure of private dependencies. It would also cause churn in the public repository, as every private integration change would require a corresponding merge. By contrast, wrapper repositories externalize these integrations, ensuring the public repository remains minimal, stable, and free from knowledge of internal structures.

2. **Why dev\_dependencies for private modules?**
   Bazel requires that all dependencies declared in the module resolution graph be resolvable. By marking private dependencies as dev\_dependencies, they do not participate in resolution for consumers who are not explicitly opting into them. This allows Party A to build successfully without private repositories while still leaving a pathway for Party B and Party C to override them in their wrappers.

3. **Why conditional activation of private dependencies?**
   Without conditionals, any target that references a private dependency would fail to build for parties that do not have access to it. Conditionals allow the same BUILD files to be used across all parties, activating private integrations only when explicitly enabled. This avoids fragmentation of the build definitions.

4. **Alternative: forked source trees per party.**
   Another option would be to give each party its own fork of the public repository where it can integrate its private dependencies. While this isolates concerns, it fragments the ecosystem, increases divergence risk, and raises maintenance costs. Wrapper repositories achieve the same isolation without forking the core public codebase.

5. **Alternative: public repository declares all dependencies (public and private).**
   If the public repository declared private dependencies directly, then Party A and external users would be unable to build without access to those private repositories. This violates the requirement that public builds must remain self-sufficient. Furthermore, it would expose internal structures.

---

### Resulting Pattern

* The **public repository** declares only public dependencies and optional stubs for private ones using dev\_dependency semantics.
* **Party A** consumes the public repository directly, with no knowledge of private dependencies.
* **Party B** uses a wrapper repository that imports the public repository and overrides the private\_b module to point to its actual source.
* **Party C** uses a wrapper repository that imports the public repository and overrides both private\_b and private\_c to point to their sources.
* Build targets that depend on private dependencies are conditionally enabled based on flags provided by the consuming party.

---

### Benefits

* Public repository is stable, minimal, and free of private churn.
* Internal dependency structures of Party B and C are not exposed to Party A or to the public.
* Each party retains full control over how to integrate private dependencies through wrapper repositories.
* Parties can evolve independently while still consuming the shared public codebase.

---

Would you like me to extend this specification with a **step-by-step process** for how each party sets up its wrapper repo (e.g., setup, override, flag configuration), so it can be directly handed to engineers to implement?
