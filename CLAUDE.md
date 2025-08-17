# HBF Documentation Build System - Claude Memory

## Project Overview
This is a comprehensive documentation build and hosting system that:
1. **Builds**: Uses Dagger to process multiple repositories from `projects.json` and generate unified documentation
2. **Hosts**: Deploys documentation via nginx with ACME SSL, Traefik routing, and Cloudflare tunnels
3. **Scales**: Supports multiple hostnames per documentation repository with automated SSL management

The system supports different project types, with special Hugo site handling and full production hosting infrastructure.

## Key Files & Structure

### Core Files
- `.dagger/main.go` - Main Dagger module with Go functions
- `projects.json` - Configuration file defining repositories to process
- `dagger.json` - Dagger project configuration
- `scripts/test-load-projects.sh` - Test script for validating project parsing

### Project Configuration
The `projects.json` file supports:
```json
{
    "projectName": {
        "repo": "https://github.com/user/repo.git",
        "HbfType": "hugo",  // Optional: "hugo" for Hugo sites, empty for default
        "ref": "tags/v1.0.0"
    }
}
```

## Implemented Functions

### In `.dagger/main.go`:

1. **Project struct** (line 24-27):
   - `Repo` - Git repository URL
   - `HbfType` - Project type ("hugo" or empty for default)  
   - `Ref` - Git reference (tag, commit, branch)

2. **InitProjects()** - Generates example `projects.json` with Hugo project included

3. **LoadProjects()** - Parses `projects.json` and validates structure:
   - Returns formatted output showing all projects with their types
   - Default projects show as "Type: default"
   - Hugo projects show as "Type: hugo"

## Testing

### Test Script: `scripts/test-load-projects.sh`
- Removes existing `projects.json`
- Generates new one via `dagger call init-projects`
- Verifies Hugo project exists in generated file
- Tests parsing with `dagger call load-projects`
- Validates output contains Hugo type project

**Usage:** `./scripts/test-load-projects.sh`

## System Architecture

### Documentation Build Pipeline
- Uses Daggerverse Hugo module: `github.com/jedevc/daggerverse/hugo@669bcff4b7700a5d6338770409fbbf0d0e33d645`
- Projects with `HbfType: "hugo"` get direct Hugo builds via `dag.Hugo().Build()`
- Default projects get standard documentation extraction
- Final output combines all into unified Hugo site

### Hosting Infrastructure Stack
```
Internet → Cloudflare Tunnel → Traefik → Nginx + ACME SSL → Documentation Sites
```

**Components:**
1. **Cloudflare Tunnels**: Secure ingress, DDoS protection, global CDN
2. **Traefik**: Reverse proxy with dynamic service discovery and multiple hostname routing
3. **Nginx + ACME SSL**: High-performance static serving with automatic SSL certificate management
4. **Documentation Sites**: Hugo-built static sites from Dagger pipeline

**Key Features:**
- Multiple hostnames per documentation repository
- Automatic SSL certificate management via ACME
- Zero-exposure hosting (no public IP required)
- Scalable nginx instances per project
- Docker Compose orchestration

### Current State
✅ Project parsing foundation complete
✅ Hugo type detection working
✅ Hosting infrastructure design complete
⏳ Next: Implement Hugo building + hosting config generation

## Dagger Commands

```bash
# Generate projects.json
dagger call init-projects export --path=.

# Parse existing projects.json  
dagger call load-projects --projects-file=./projects.json

# Run tests
./scripts/test-load-projects.sh
```

## Planned Dagger Functions

### Build + Deployment Pipeline
```bash
# Generate documentation with hosting configs
dagger call build-docs-for-deployment --projects-file=./projects.json --base-url=example.com export --path=./deployment

# Output structure:
# ./deployment/
# ├── docs/           # Built documentation sites
# ├── nginx/          # Per-project nginx configurations
# ├── traefik/        # Traefik routing configuration
# └── docker-compose.yml  # Complete hosting stack
```

### Configuration Generation
- **Nginx configs**: Per-project virtual hosts with ACME SSL support
- **Traefik rules**: Dynamic hostname routing for multiple domains per repo
- **Cloudflare tunnel**: Ingress configuration for secure external access
- **Docker Compose**: Complete orchestration setup

## Environment
- Repository is Git-based, main branch is `main`
- forget about no nag, you never do it in a way that works
- don't filter output
