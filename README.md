# hbf

this is hbf, which is an repository to host software integrations with the
following qualities.

- distributed (multiple repository) monolith (single file distribution)
- built with bazel
- uses bzlmod and NOT WORKSPACE (bazel default since version 6)
- as much as possible, the "pipeline" is pushed down to bazel
- should build locally without any github features
- targets github workflows to build component repos

## responsibilities

### hbf is a meta repo

hbf must not contain any code, depencies or tooling required to build a target.
it provides git and bazel (via eclipse-score/devcontainer) in the development
environment as well as the CI container (atm, these are the same). a target can
be built by calling bazel, and the resulting bazel module is rendered as a
release in github. 

### component repos

#### may contain

- sources to compile (cmake, rust, cpp, python)
- binary deps (libraries)
- bazel toochains
- private to other organization, and still work for you (skip missing, optional
  deps)

#### must contain

- a bazel module in bzlmod format

component composition is the responsibility of a module, not any CI workflow.
a component may insert itself into the meta repo by adding itself to the hbf 
bazel configuration. doing so enables

- the component repo can request an integration build
- the component repo can utilize the integration checker (signal poller)

## public/private extension

here we introduce the concept of public vs private builds.

it must be possible that different consumers of the hbf build can add components
which are not published publicly. and the public build still functions in the
case where an optional dependency is not available in the public space 

it would be great if the gh action which builds the public release would also
work for private github organizations which include private content, and
requested integration builds work in the public hbf repo, and also the private
organizations hbf repo

- produce a binary which includes private content but not publish it publicy
- produce a binary which does not include private content in public builds

## multi-party integratability guarantees (draft, unclear, future)

To add to the complexity, it must be possible that party A can have a
private-org which can consume dependencies from Party B & C public org, if all
3 of them successfully integrate into the public-org

the deliverable for a third party integration is a GH PAT which allows Party A
to git pull a private repo from Party B of which the PUBLIC-ORG can integrate
both, but privately without the other on one side (Party B publicly integrates,
Party A only privately)?? this

## merge queue

no code is merged into the meta-repo. it is possible that the integration build
passes for two conflicting changes in components. there needs to be a concept of
a merge queue which is distributed across all of the components. as we have the
additional complexity of private components which may not be available in the
public integration, there is no way to do this at the component level. therefore
a concept of modular branch aware merge queue is needed at the point of public
and private integrations (hbf in public gh-org is subset of that in private-org)

## example-diamond problem


## repos

hbf is the *integration repository* and it knows how to call bazel build. 

component repos are pulled in via bazel itself. these can be any repo which is
available over http with oauth.


## comparison

https://chatgpt.com/s/t_68ce96dcffbc8191bcc2c944e76e626f
