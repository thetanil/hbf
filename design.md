# Documentation Build System Design

## Overview

This design outlines a Dagger-based documentation build system that:
1. Reads project configurations from `projects.json`
2. Checks out specified repositories and refs
3. Generates markdown documentation
4. Renders documentation with Hugo
5. Exports the final output to the host filesystem for inspection

## Architecture

### Input: projects.json Structure
The system uses the existing `projects.json` format with optional `HbfType` field:
```json
{
    "projA": {
        "repo": "https://github.com/dagger/dagger.git",
        "ref": "196f232a4d6b2d1d3db5f5e040cf20b6a76a76c5"
    },
    "projB": {
        "repo": "https://github.com/example/repoB.git", 
        "ref": "tags/v0.0.1"
    },
    "projB-docs": {
        "repo": "https://github.com/example/repoB-docs.git",
        "HbfType": "hugo",
        "ref": "tags/v0.0.1"
    }
}
```

### Dagger Function Design

#### 1. BuildDocs Function
```go
func (m *Hbf) BuildDocs(ctx context.Context, projectsFile *dagger.File) (*dagger.Directory, error)
```

**Pipeline Steps:**
1. **Parse Configuration**: Read and parse `projects.json`
2. **Repository Checkout**: For each project, clone the specified repo at the given ref
3. **Documentation Generation**: Extract/generate markdown documentation from each repository
4. **Hugo Site Assembly**: Create Hugo site structure with generated content
5. **Hugo Build**: Render markdown to HTML using Hugo
6. **Output Directory**: Return directory containing built documentation

#### 2. Detailed Implementation Flow

**Step 1: Configuration Parsing**
- Mount `projects.json` file into container
- Parse JSON into Go structs using existing `Config` type
- Validate repository URLs and refs

**Step 2: Repository Management & Type-Based Processing**
```go
for projectName, project := range config {
    // Clone repository at specific ref
    repoDir := dag.Git(project.Repo).Tag(project.Ref).Tree()
    
    // Handle different project types
    switch project.HbfType {
    case "hugo":
        // Execute Hugo build procedure for Hugo sites
        builtSite := buildHugoSite(ctx, repoDir, projectName)
        // Copy built Hugo site to final output
        
    default:
        // Standard documentation extraction for non-Hugo repos
        docsDir := extractDocs(ctx, repoDir, projectName)
    }
}
```

**Step 3: Hugo Site Processing (HbfType: "hugo")**
For repositories with `HbfType: "hugo"`:
1. **Detect Hugo Site**: Verify presence of `config.yaml`/`config.toml` and `content/` directory
2. **Install Dependencies**: Run `npm install` or theme installation if needed
3. **Hugo Build**: Execute `hugo --minify` to build the site
4. **Extract Output**: Copy the generated `public/` directory
5. **Integration**: Mount built site into final documentation structure

**Step 4: Standard Documentation Extraction (Default)**
For repositories without `HbfType` or other types:
- Scan each repository for documentation files (README.md, docs/, etc.)
- Extract code comments and generate API documentation  
- Create project-specific markdown files
- Generate index and navigation structure

**Step 5: Final Site Structure**
```
content/
├── _index.md          # Main landing page
├── projA/
│   ├── _index.md      # Project A overview
│   ├── api.md         # Generated API docs
│   └── readme.md      # Repository README
├── projB/
│   ├── _index.md
│   ├── api.md
│   └── readme.md
└── projB-docs/        # Hugo site content (HbfType: "hugo")
    └── (built Hugo site files mounted here)

static/                # Static assets
themes/                # Hugo theme
config.yaml           # Hugo configuration
```

**Step 6: Hugo Build Process Using Daggerverse**

**Daggerverse Hugo Module Installation:**
```
dagger install github.com/jedevc/daggerverse/hugo@669bcff4b7700a5d6338770409fbbf0d0e33d645
```

**For Hugo Sites (HbfType: "hugo"):**
```go
func buildHugoSite(ctx context.Context, repoDir *dagger.Directory, projectName string) *dagger.Directory {
    // Build Hugo site using Daggerverse Hugo module
    return dag.Hugo().Build(repoDir, dagger.HugoBuildOpts{
        Minify: true,
        HugoVersion: "latest",
        DartSassVersion: "latest",
    })
}
```

**For Final Site Assembly:**
```go
// Build final aggregated site using Daggerverse Hugo module
return dag.Hugo().Build(aggregatedSiteDir, dagger.HugoBuildOpts{
    Minify: true,
    HugoVersion: "latest",
    BaseUrl: "https://docs.example.com", // Optional base URL
    HugoEnv: "production", // Optional environment
})
```

### Export to Host Filesystem

The final built documentation directory can be exported to the host using:
```bash
dagger call build-docs --projects-file=./projects.json export --path=./docs-output
```

### Hugo Configuration

**config.yaml**:
```yaml
baseURL: "/"
languageCode: "en-us"
title: "Multi-Project Documentation"
theme: "docsy"  # Or custom theme

params:
  version: "latest"
  github_repo: "https://github.com/your-org/docs"
  
menu:
  main:
    - name: "Projects"
      url: "/projects/"
      weight: 10

markup:
  goldmark:
    renderer:
      unsafe: true
  highlight:
    style: "github"
    lineNos: true
```

### Error Handling & Validation

- **Repository Access**: Validate repository URLs and handle authentication
- **Ref Resolution**: Ensure specified refs/tags exist
- **Documentation Quality**: Validate generated markdown syntax
- **Hugo Build**: Handle Hugo build errors and missing dependencies

### Project Type Handling

The system supports different `HbfType` values:

**HbfType: "hugo"**
- Repository contains a complete Hugo site
- System builds the Hugo site directly using `hugo --minify`
- Output is mounted into the final documentation structure
- No additional processing or content extraction needed

**HbfType: "" (default/empty)**
- Standard documentation extraction workflow
- Scans for README.md, docs/ folders, code comments
- Generates markdown files for integration into main Hugo site
- Content is processed and themed consistently

### Implementation Notes

**Dagger Module Dependencies:**
Add to `dagger.json`:
```json
{
  "name": "hbf",
  "dependencies": [
    {
      "name": "hugo",
      "source": "github.com/jedevc/daggerverse/hugo@669bcff4b7700a5d6338770409fbbf0d0e33d645"
    }
  ]
}
```

**Go Struct Extension Required:**
```go
type Project struct {
    Repo    string `json:"repo"`
    HbfType string `json:"HbfType,omitempty"`
    Ref     string `json:"ref"`
}
```

**Build Workflow:**
1. Parse `projects.json` with HbfType support
2. For each project, check `HbfType` field
3. Route to appropriate build procedure:
   - Hugo sites → Daggerverse Hugo build (`dag.Hugo().Build()`)
   - Default → Documentation extraction + Hugo integration
4. Combine all outputs into final site structure
5. Export to host filesystem

**Daggerverse Integration Benefits:**
- **Standardized Hugo builds**: Consistent Hugo version and build process
- **Dependency management**: Automatic Hugo and Dart Sass installation
- **Build optimizations**: Built-in minification and production settings
- **Version control**: Pin specific Hugo versions per project
- **Simplified code**: Less custom container management

### Extensibility

The system supports:
- **Multiple Project Types**: Hugo sites, standard repos, API docs
- **Custom Documentation Extractors**: Plugin system for different project types  
- **Theme Customization**: Configurable Hugo themes and layouts
- **Multi-format Output**: PDF, EPUB generation alongside HTML
- **Incremental Builds**: Cache repository checkouts for faster rebuilds

### Usage Example

```bash
# Install Daggerverse Hugo module
dagger install github.com/jedevc/daggerverse/hugo@669bcff4b7700a5d6338770409fbbf0d0e33d645

# Build documentation for all projects
dagger call build-docs --projects-file=./projects.json export --path=./site

# Build with custom Hugo settings
dagger call build-docs --projects-file=./projects.json --base-url=https://docs.example.com --hugo-env=production export --path=./site

# Build specific project only
dagger call build-docs --projects-file=./projects.json --filter=projA export --path=./projA-docs
```

## Hosting Infrastructure

### Architecture Overview

The documentation hosting stack consists of multiple layers for scalable, secure deployment:

```
Internet → Cloudflare Tunnel → Traefik → Nginx + ACME SSL → Documentation Sites
```

### Components

#### 1. Cloudflare Tunnels
- **Purpose**: Secure ingress without exposing server IPs
- **Configuration**: Multiple tunnel hostnames per documentation repository
- **Benefits**: DDoS protection, global CDN, zero-trust access

#### 2. Traefik (Reverse Proxy & Load Balancer)
- **Role**: Routes traffic to appropriate nginx instances
- **Features**: 
  - Dynamic service discovery
  - Multiple hostname routing per documentation repo
  - Integration with Cloudflare tunnel endpoints
- **Configuration**: Docker labels or file-based routing rules

#### 3. Nginx + ACME SSL Module
- **Purpose**: High-performance static file serving with automatic SSL
- **ACME Integration**: Automatic SSL certificate management
- **Features**:
  - HTTP/2 and HTTP/3 support
  - Gzip compression for documentation assets
  - Custom headers and caching policies
  - Multiple virtual hosts per documentation repository

#### 4. Documentation Sites
- **Content**: Hugo-built static sites from Dagger pipeline
- **Structure**: Each project gets dedicated nginx virtual host
- **Deployment**: Volume mounts or direct file deployment

### Deployment Workflow

#### 1. Documentation Generation
```go
// Extended BuildDocs function with deployment output
func (m *Hbf) BuildDocsForDeployment(ctx context.Context, projectsFile *dagger.File, baseUrl string) (*dagger.Directory, error) {
    // Build documentation using existing pipeline
    docsDir := m.BuildDocs(ctx, projectsFile)
    
    // Generate nginx configuration for each project
    nginxConfig := generateNginxConfig(ctx, projectsFile, baseUrl)
    
    // Generate Traefik configuration
    traefikConfig := generateTraefikConfig(ctx, projectsFile)
    
    // Return directory with docs + configs
    return dag.Directory().
        WithDirectory("/docs", docsDir).
        WithDirectory("/nginx", nginxConfig).
        WithDirectory("/traefik", traefikConfig), nil
}
```

#### 2. Nginx Configuration Generation
For each project in `projects.json`:
```nginx
server {
    listen 80;
    server_name docs-projA.example.com docs-projA-alt.example.com;
    
    # ACME SSL configuration
    include /etc/nginx/acme-challenge.conf;
    
    location / {
        root /var/www/projA;
        index index.html;
        
        # Hugo-optimized settings
        location ~* \.(css|js|png|jpg|jpeg|gif|ico|svg)$ {
            expires 1y;
            add_header Cache-Control "public, immutable";
        }
        
        # HTML files with shorter cache
        location ~* \.html$ {
            expires 1h;
            add_header Cache-Control "public";
        }
        
        # Fallback for Hugo pretty URLs
        try_files $uri $uri/ /index.html;
    }
}
```

#### 3. Traefik Configuration
```yaml
http:
  routers:
    projA-docs:
      rule: "Host(`docs-projA.example.com`) || Host(`docs-projA-alt.example.com`)"
      service: projA-nginx
      tls:
        certResolver: cloudflare
        
  services:
    projA-nginx:
      loadBalancer:
        servers:
          - url: "http://nginx-projA:80"
```

#### 4. Cloudflare Tunnel Configuration
```yaml
tunnel: <tunnel-id>
credentials-file: /etc/cloudflared/credentials.json

ingress:
  - hostname: docs-projA.example.com
    service: http://traefik:80
  - hostname: docs-projA-alt.example.com  
    service: http://traefik:80
  - hostname: docs-projB.example.com
    service: http://traefik:80
  - service: http_status:404
```

### Container Orchestration

#### Docker Compose Structure
```yaml
version: '3.8'
services:
  cloudflared:
    image: cloudflare/cloudflared:latest
    command: tunnel --config /etc/cloudflared/config.yml run
    volumes:
      - ./cloudflared:/etc/cloudflared
      
  traefik:
    image: traefik:latest
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./traefik:/etc/traefik
      - /var/run/docker.sock:/var/run/docker.sock
      
  nginx-projA:
    image: nginx:alpine
    volumes:
      - ./docs/projA:/var/www/projA
      - ./nginx/projA.conf:/etc/nginx/conf.d/default.conf
    labels:
      - "traefik.enable=true"
      - "traefik.http.routers.projA.rule=Host(`docs-projA.example.com`)"
      
  nginx-projB:
    image: nginx:alpine  
    volumes:
      - ./docs/projB:/var/www/projB
      - ./nginx/projB.conf:/etc/nginx/conf.d/default.conf
```

### SSL Certificate Management

#### ACME Module Configuration
```nginx
# /etc/nginx/acme-challenge.conf
location ^~ /.well-known/acme-challenge/ {
    root /var/www/acme;
    try_files $uri =404;
}

# Auto-redirect HTTP to HTTPS after cert acquisition
if ($scheme != "https") {
    return 301 https://$server_name$request_uri;
}
```

#### Certificate Automation
- **Challenge Type**: HTTP-01 via nginx
- **Renewal**: Automated via cron or systemd timers
- **Storage**: Shared volume between nginx containers
- **Monitoring**: Certificate expiry alerts

### Scaling Considerations

#### Multi-Repository Support
- **Dynamic nginx configs**: Generated per project
- **Traefik auto-discovery**: Docker labels or file watchers
- **Resource isolation**: Separate nginx containers per major project
- **Shared assets**: Common CSS/JS served from CDN

#### Performance Optimization
- **nginx tuning**: Worker processes, connection limits
- **Caching strategy**: Browser cache + optional Redis for dynamic content
- **Compression**: Gzip/Brotli for text assets
- **HTTP/3**: QUIC support for modern clients

### Monitoring & Observability

#### Health Checks
- **Traefik**: Built-in dashboard and metrics
- **nginx**: Status endpoints and access logs
- **Cloudflare**: Tunnel status monitoring
- **SSL**: Certificate expiry tracking

#### Logging Strategy
- **Centralized logs**: All containers → syslog or ELK stack
- **Access patterns**: nginx access logs for analytics
- **Error tracking**: Application and infrastructure errors
- **Performance metrics**: Response times, cache hit rates

### Integration Benefits

- **Reproducible Builds**: Containerized environment ensures consistent documentation
- **Version Control**: Pin specific repository refs for stable documentation
- **CI/CD Integration**: Automated documentation updates on repository changes
- **Multi-Repository Support**: Aggregate documentation from multiple sources
- **Scalable Hosting**: Auto-scaling nginx instances per project
- **Secure Access**: Cloudflare tunnels eliminate IP exposure
- **SSL Automation**: Zero-touch certificate management
- **Global Performance**: Cloudflare CDN acceleration