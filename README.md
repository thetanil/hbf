# hbf

need taskfile for a watcher. skip makefile. cli with what then? pocketci?

no - locked to github
yes - right model. it's a tool for any repo
yes - the tool should be able to build itself

https://github.com/franela/pocketci



https://github.com/playgroundtech/dagger-go-example-app/blob/main/README.md

playgroundtech/dagger-go-example-app

Description: A CLI-style example Go app that fetches a random dad joke, with
workflows using Dagger, GoReleaser, and GitHub Actions. Offers both PR
(“pull-request”) and release pipelines, callable via go run ./ci/dagger.go
--local pull-request. GitHub

Organization: Has ci/dagger.go, a defined flowchart for Dagger pipelines, and
clear separation between test and release tasks.

Why it qualifies as CLI-ready: Concrete command-line argument workflow
integration and polished automation.