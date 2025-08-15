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

project init
repo link <url> 

checkout repo i guess, but i will run it from a repo root i hope

point to reach is that hbf can build itself as a go app?
no no, you check this out and it builds any repo with dagger

hbf project dag
hbf repo list
hbf repo edit
hbf repo add
hbf svc list
hbf svc add
hbf tool add <url> <tooltype>
hbf <tooltype>
hbf build
hbf test
hbf release

repo links url to fs:/home/user/asdf
svc links repo to deployment (long running fn, service registration)
tool links repo to dagger support utils
https://github.com/kpenfound/hello-monorepo

## cli v2

INI-like config:

```ini
[section1]
var1=value1
var2=value2

[section2]
var1=foo
var2=bar
```

shell function to parse it

```sh
parse_ini() {
    section=""
    while IFS= read -r line; do
        case $line in
            \[*\]) section="${line#[}"; section="${section%]}";;
            ""|\#*) continue;;
            *=*)
                key="${line%%=*}"
                val="${line#*=}"
                key="$(echo "$key" | tr -d '[:space:]')"
                val="$(echo "$val" | sed 's/^ *//; s/ *$//')"
                eval "${section}_${key}=\$val"
                ;;
        esac
    done < "$1"
}
```

the main hbf script will have a function to expand templates into commands
hbf has available based on the config ini file

```sh
expand_template() {
    eval "echo \"$1\""
}
expand_template "Hello, \$section2_var1 from \$section2_var2"
```

main script ./hbf uses this case switch style command line processing, but it
should also generate some command dynamically based on the parsed ini content.

```sh
# Main function
hbf_main() {
    # Path to the dagger script
    local dagger_script="$(dirname "$0")/scripts/dagger.sh"
    
    case "$1" in
        dagger-all)
            "$dagger_script" all
            ;;
        dagger-install)
            "$dagger_script" install
```

cli completion is always available via ./hbf --complete and the COMMANDS
variable also needs to be updated given the commands generated based on the ini
config. 

```sh
# Define available commands based on Makefile targets
COMMANDS="dagger-all dagger-install dagger-service dagger-start dagger-stop dagger-status dagger-logs dagger-test dagger-cache-test dagger-uninstall help"

# Auto-register completion when script is sourced or executed
if [[ "${BASH_SOURCE[0]}" != "${0}" ]] || [[ "${COMP_WORDS}" ]]; then
    # We're being sourced or called for completion
    _hbf_completion() {
        local cur="${COMP_WORDS[COMP_CWORD]}"
        COMPREPLY=( $(compgen -W "${COMMANDS}" -- "${cur}") )
    }
    complete -F _hbf_completion "${BASH_SOURCE[0]##*/}"
    complete -F _hbf_completion hbf
    return 2>/dev/null || exit 0
fi
```

templates will be go modules which are run like:
```sh
go run ./{project_name}/dagger.go --local pull-request
or what about python
dagger run 
```

types in ini file identify the project type
project name is what identifies the section

[libglib]
type=lib
active=false
latest_tag=
path=<type>/libglib
repo_url=
publish=
version=

[syft]
type=tool
version=1.3.5
dl_url=
license=


https://github.com/kpenfound/hello-monorepo