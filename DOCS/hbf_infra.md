# HBF Infrastructure & Deployment Design

## Overview

HBF is designed for flexible deployment scenarios:
- **Multi-tenant hosted service**: Serving thousands of henv databases from a cluster
- **Self-hosted installations**: Single or small number of henv databases
- **Local development**: Single henv on developer workstation (Linux/Windows)

This document describes the infrastructure architecture, deployment patterns, and operational considerations for each scenario.

---

## Core Concepts

### HBF Binary
The single, statically-linked C99 binary that serves HTTP requests and manages compute environments.

### Henv (HBF Environment)
A complete web application packaged as a single SQLite database file containing:
- Application code (QuickJS modules)
- User data and schemas
- Documents and FTS5 search indices
- System configuration and permissions
- Static assets and templates

### Database Naming Convention
Henv files are named using an 8-character DNS-safe hash derived from the hostname:

```python
import hashlib

def dns_safe_short_hash(s, length=8):
    """Generate DNS-safe short hash for henv filename"""
    digest = hashlib.sha256(s.encode()).digest()
    num = int.from_bytes(digest, 'big')
    chars = '0123456789abcdefghijklmnopqrstuvwx'
    out = ''
    while num > 0 and len(out) < length:
        num, i = divmod(num, 36)
        out = chars[i] + out
    return out.rjust(length, '0')

# Example: dns_safe_short_hash("myapp") → "1c8b2f3a"
# Filename: 1c8b2f3a.db
```

**Properties:**
- DNS-safe characters only (0-9, a-z)
- 8 characters = ~2.8 trillion possible values
- Low collision probability for thousands of databases
- Can be used as subdomain prefix

---

## Hosted Service Architecture

### High-Level Design

```
Internet
    ↓
Cloudflare Tunnel (cloudflared)
    ↓
Traefik Ingress (TLS termination + routing)
    ↓
HBF Pods (1 or more, sticky sessions)
    ↓
Henv Files (mounted persistent storage)
```

### Infrastructure Components

#### 1. Hardware Platform
- **Initial deployment**: 4x Raspberry Pi nodes running k3s
- **Node pool**: Lightweight Kubernetes cluster
- **Storage**: Local SSD/NVMe per node (no NFS/network storage)

#### 2. Cloudflare Tunnel Integration
- **Component**: `cloudflared` daemon running as DaemonSet or standalone pod
- **Configuration**: Single tunnel endpoint connected to k3s cluster
- **SSL Mode**: "Full" (Cloudflare ↔ Traefik encrypted)
- **DNS**: Wildcard CNAME for `*.ipsaw.com → tunnel-id.cfargotunnel.com`

**Wildcard Domain Support:**
```yaml
# Cloudflare Tunnel config.yml
tunnel: <tunnel-id>
credentials-file: /etc/cloudflared/credentials.json

ingress:
  - hostname: "*.ipsaw.com"
    service: https://traefik.kube-system.svc.cluster.local:443
    originRequest:
      noTLSVerify: false
  - hostname: "ipsaw.com"
    service: https://traefik.kube-system.svc.cluster.local:443
  - service: http_status:404
```

**Custom User Domains:**
For users with their own domains (e.g., `myapp.example.com`):
1. User creates CNAME: `myapp.example.com → {hash}.ipsaw.com`
2. User adds custom domain to their henv configuration via admin panel
3. Traefik routes based on SNI/Host header to correct henv
4. Cloudflare Tunnel handles all `*.ipsaw.com` traffic + custom domains pointing to it

**Limitations:**
- Custom domains must CNAME to `*.ipsaw.com` (cannot point directly to tunnel)
- User must own/control DNS for custom domain
- SSL certificate via Let's Encrypt (Traefik ACME)

#### 3. Traefik Configuration
- **Role**: Ingress controller + TLS termination + routing
- **TLS**: Let's Encrypt ACME with wildcard certificate for `*.ipsaw.com`
- **Routing strategy**: HTTP Host header → henv database selection

**Traefik IngressRoute Example:**
```yaml
apiVersion: traefik.containo.us/v1alpha1
kind: IngressRoute
metadata:
  name: hbf-wildcard
  namespace: hbf
spec:
  entryPoints:
    - websecure
  routes:
    - match: HostRegexp(`{subdomain:[a-z0-9]{8}}.ipsaw.com`)
      kind: Rule
      services:
        - name: hbf
          port: 80
  tls:
    certResolver: letsencrypt
    domains:
      - main: ipsaw.com
        sans:
          - "*.ipsaw.com"
```

**Host Header Forwarding:**
Traefik must preserve the original `Host` header so HBF can determine which henv to load:
```yaml
# Traefik middleware to preserve Host header
apiVersion: traefik.containo.us/v1alpha1
kind: Middleware
metadata:
  name: preserve-host
spec:
  headers:
    customRequestHeaders:
      X-Forwarded-Host: ""  # Use original Host
```

#### 4. HBF Deployment

**Kubernetes Deployment:**
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: hbf
  namespace: hbf
spec:
  replicas: 1  # Start with 1, scale horizontally later
  selector:
    matchLabels:
      app: hbf
  template:
    metadata:
      labels:
        app: hbf
    spec:
      containers:
      - name: hbf
        image: ghcr.io/yourorg/hbf:latest
        args:
          - --port=80
          - --storage_dir=/data/henvs
          - --log_level=info
          - --db_max_open=100
          - --qjs_mem_mb=64
          - --qjs_timeout_ms=5000
        ports:
        - containerPort: 80
          name: http
        volumeMounts:
        - name: henv-storage
          mountPath: /data/henvs
        resources:
          requests:
            memory: "512Mi"
            cpu: "250m"
          limits:
            memory: "2Gi"
            cpu: "1000m"
        livenessProbe:
          httpGet:
            path: /health
            port: 80
          initialDelaySeconds: 10
          periodSeconds: 30
        readinessProbe:
          httpGet:
            path: /health
            port: 80
          initialDelaySeconds: 5
          periodSeconds: 10
      volumes:
      - name: henv-storage
        persistentVolumeClaim:
          claimName: henv-pvc
```

**Service Definition:**
```yaml
apiVersion: v1
kind: Service
metadata:
  name: hbf
  namespace: hbf
spec:
  selector:
    app: hbf
  ports:
  - port: 80
    targetPort: 80
    protocol: TCP
    name: http
  type: ClusterIP
```

**Storage:**
```yaml
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: henv-pvc
  namespace: hbf
spec:
  accessModes:
    - ReadWriteOnce  # Single node access
  resources:
    requests:
      storage: 100Gi
  storageClassName: local-path  # k3s default
```

---

## Database Distribution Strategy

### Single-Node Deployment (Phase 1)
- **All henv files on one node**: Simplest setup, no distribution complexity
- **Storage**: Local SSD mounted to single HBF pod
- **Scaling limit**: Vertical scaling only (CPU/memory/disk)

### Multi-Node Deployment (Future)
When scaling beyond one node, consider:

**Option A: Consistent Hashing + Sticky Sessions**
1. Hash hostname to determine node assignment
2. Traefik uses consistent hash algorithm to route to correct pod
3. Each pod mounts storage with subset of henv files
4. Requires pod affinity + node-local storage

**Option B: Shared Storage with Locking**
1. All pods access shared storage (NFS/Ceph/Longhorn)
2. SQLite WAL mode with proper locking
3. Simpler routing, but network storage overhead
4. **Trade-off**: Violates "no NFS" preference from design

**Option C: Database Affinity with Dynamic Routing**
1. Maintain routing table (henv hash → node IP)
2. Traefik queries routing service before forwarding
3. L7 routing based on hostname lookup
4. Requires service discovery layer

**Recommended Approach**: Start with Option A when scaling is needed. Use k3s node affinity + local-path storage for database pinning.

---

## Henv Lifecycle Management

### Creation Flow

**Endpoint**: `POST /new`

1. User visits `ipsaw.com/new`
2. Authenticates with OAuth provider (Google/GitHub)
3. Enters desired hostname: `{userhostname}`
4. System generates:
   - Hash: `dns_safe_short_hash(userhostname)` → `{hash}`
   - Filename: `{hash}.db`
   - Hostname: `{hash}.ipsaw.com` (or custom domain)
5. HBF creates new SQLite database with initial schema
6. Owner account created with provided email + password
7. Response includes:
   - Full URL: `https://{hash}.ipsaw.com`
   - Admin credentials
   - Litestream replication token (for backup/sync)

**Initial Schema:**
- System tables (`_hbf_*`)
- Owner user with `admin` role
- Default permissions
- Welcome document/template

### Inactivity Policy

**Rules:**
- **Inactivity threshold**: 30 days with no database writes
- **Warning**: Email to owner at 30 days
- **Deletion**: Automatic deletion at 60 days (30 days after warning)

**Tracking:**
- `_hbf_config` table stores `last_activity_ts` (Unix timestamp)
- Updated on any write operation (INSERT/UPDATE/DELETE)
- Cron job checks all henv files daily for inactivity

**Email Notification Service:**
```bash
# Daily cron job
#!/bin/bash
# Check all .db files in /data/henvs
# Query last_activity_ts from _hbf_config
# Send warning emails via SMTP/SendGrid
# Delete henv files past deletion threshold
```

### Backup & Replication

**Litestream Integration:**
- Each henv file replicated to S3/Backblaze/local storage
- Continuous replication (sub-second granularity)
- Point-in-time restore capability
- Enables local sync for offline development

**Litestream Config Example:**
```yaml
dbs:
  - path: /data/henvs/*.db
    replicas:
      - type: s3
        bucket: hbf-backups
        path: henvs
        region: us-east-1
        retention: 168h  # 7 days
```

**User Download/Sync:**
Users can sync their henv to local machine:
```bash
# Install litestream + hbf binary
litestream replicate -config litestream.yml \
  s3://hbf-backups/henvs/{hash}.db \
  ./local-henvs/{hash}.db

# Run locally
./hbf --storage_dir=./local-henvs --port=8080
```

---

## Self-Hosting Requirements

### Minimal Deployment
For users running HBF on their own infrastructure:

**Requirements:**
- Linux or Windows PC/server
- HBF static binary (single file, ~10-20MB)
- Henv database file(s)
- Optional: Reverse proxy for HTTPS (nginx, Caddy)

**Example Setup:**
```bash
# Download HBF binary
curl -LO https://github.com/yourorg/hbf/releases/latest/download/hbf-linux-amd64
chmod +x hbf-linux-amd64

# Create storage directory
mkdir -p ./henvs

# Initialize new henv or download existing
./hbf-linux-amd64 --storage_dir=./henvs --port=5309

# Access locally
curl http://localhost:5309/new
```

**With Reverse Proxy (Caddy):**
```caddyfile
# Caddyfile
*.example.com {
    reverse_proxy localhost:5309
    tls {
        dns cloudflare {env.CLOUDFLARE_API_TOKEN}
    }
}

example.com {
    reverse_proxy localhost:5309
}
```

**Systemd Service (Linux):**
```ini
# /etc/systemd/system/hbf.service
[Unit]
Description=HBF Application Server
After=network.target

[Service]
Type=simple
User=hbf
WorkingDirectory=/opt/hbf
ExecStart=/opt/hbf/hbf --storage_dir=/var/lib/hbf/henvs --port=5309
Restart=on-failure
RestartSec=10s

[Install]
WantedBy=multi-user.target
```

---

## Development Workflow

### Live Development with Monaco
Users can develop directly in the browser:
1. Navigate to `https://{hash}.ipsaw.com/admin`
2. Use Monaco editor (served from henv database itself)
3. Edit QuickJS modules, templates, documents
4. Changes persisted immediately to SQLite
5. Live reload for instant feedback

**Smalltalk-Like Self-Hosting:**
- The development environment is part of the application
- Monaco editor served from `_hbf_documents` or static table
- Code modifications update `_hbf_endpoints` table
- Application bootstraps itself from database

### Local Development Sync
Developers can work offline and sync:
```bash
# Pull latest from hosted service
litestream restore -o local.db s3://hbf-backups/henvs/{hash}.db

# Run locally
./hbf --storage_dir=. --port=8080 --dev

# Make changes via browser at localhost:8080

# Push changes back (manual or scheduled)
litestream replicate -config litestream.yml
```

---

## Observability & Operations

### Health Checks
**Endpoint**: `GET /health`

Response:
```json
{
  "status": "ok",
  "version": "1.0.0",
  "uptime_seconds": 86400,
  "active_henvs": 42,
  "db_connections": 15,
  "memory_mb": 512
}
```

### Metrics (Phase 10)
**Endpoint**: `GET /{cenv}/admin/metrics` (admin only)

Per-henv metrics:
- Request count/latency
- QuickJS execution time
- Database query performance
- Error rates
- Active sessions

**System-wide metrics** (Prometheus export):
```
# HELP hbf_requests_total Total HTTP requests
# TYPE hbf_requests_total counter
hbf_requests_total{henv="1c8b2f3a",method="GET",status="200"} 1523

# HELP hbf_db_connections Active database connections
# TYPE hbf_db_connections gauge
hbf_db_connections{henv="1c8b2f3a"} 3

# HELP hbf_qjs_execution_seconds QuickJS execution time
# TYPE hbf_qjs_execution_seconds histogram
hbf_qjs_execution_seconds_bucket{henv="1c8b2f3a",le="0.1"} 1200
```

### Logging
**Structured JSON logs** to stdout:
```json
{
  "timestamp": "2025-10-19T12:34:56Z",
  "level": "info",
  "henv": "1c8b2f3a",
  "method": "POST",
  "path": "/api/documents",
  "duration_ms": 45,
  "status": 201,
  "user_id": "user_123"
}
```

**Log aggregation** (hosted service):
- Kubernetes: FluentBit → Loki/Elasticsearch
- Self-hosted: Logs to file, rotate with logrotate

### Alerting
Critical alerts:
- Pod crash loop
- High error rate (>5% 5xx responses)
- Database connection exhaustion
- Disk space < 10% available
- QuickJS timeout threshold exceeded

---

## Security Considerations

### Network Security
- **TLS termination**: Traefik with Let's Encrypt
- **Internal traffic**: HTTP between Traefik ↔ HBF (k3s network policies)
- **Cloudflare protection**: DDoS mitigation, WAF rules
- **Rate limiting**: Traefik middleware per IP/hostname

### Authentication
- **Owner account**: Created on henv initialization, no recovery
- **OAuth (ipsaw.com/new)**: Google/GitHub for initial authentication
- **JWT sessions**: Stored in `_hbf_sessions`, revocable
- **Password hashing**: Argon2id (as per hbf_impl.md)

### Authorization
- **Table permissions**: `_hbf_table_permissions`
- **Row-level security**: `_hbf_row_policies`
- **QuickJS sandboxing**: Memory limits, execution timeouts, no host access

### Data Isolation
- **Per-henv database**: Complete isolation between tenants
- **No shared data**: Each henv is independent SQLite file
- **Resource limits**: QuickJS memory/timeout enforced per-request

### Backup Security
- **Litestream encryption**: S3 server-side encryption (SSE-S3 or SSE-KMS)
- **Access control**: S3 bucket policies restrict access
- **User downloads**: Authenticated token required for restore

---

## Scaling Strategy

### Vertical Scaling
Single pod/node:
- Increase CPU/memory limits
- Add storage capacity
- Optimize SQLite (WAL, cache size)
- Reasonable for 1,000-10,000 henvs

### Horizontal Scaling
Multiple pods/nodes:
- **Sticky sessions** via Traefik consistent hashing
- **Database sharding** by hash prefix (0-7a, 7b-f9, etc.)
- **Pod affinity** to co-locate henv subsets
- **Node-local storage** (avoid network overhead)

**Traefik Configuration for Consistent Hashing:**
```yaml
apiVersion: traefik.containo.us/v1alpha1
kind: Middleware
metadata:
  name: sticky-hash
spec:
  plugin:
    consistentHash:
      hashKey: "request.host"
```

### Storage Scaling
- **Local SSD per node**: Best performance, limited by node capacity
- **Distributed storage**: Longhorn/Ceph if cross-node access needed
- **Tiering**: Hot henvs on SSD, cold henvs on HDD

### Database Optimization
Per henv:
- WAL mode with `synchronous=NORMAL`
- Shared cache across connections
- Connection pooling (configurable `--db_max_open`)
- VACUUM schedule for reclaiming space

---

## Cost Estimation (Hosted Service)

### Infrastructure Costs
**Raspberry Pi Cluster (4 nodes):**
- Hardware: $200/node × 4 = $800 (one-time)
- Power: ~20W/node × 4 = 80W = ~$7/month
- Network: Cloudflare Tunnel = free (up to certain limits)
- Total monthly: ~$10-20 (power + internet)

**Storage:**
- 1TB SSD per node = $100/node × 4 = $400 (one-time)
- S3 backup (1TB): ~$23/month + egress

**Total first year**: ~$1,200 hardware + ~$500 operational = ~$1,700

### Capacity Estimate
**Per node** (8GB RAM, 1TB storage):
- ~10,000 henv files (avg 10MB each = 100GB)
- ~100 concurrent requests (depends on workload)
- Total capacity: ~40,000 henvs across 4 nodes

### Revenue Model (Optional)
If monetizing:
- Free tier: 1 henv per user, 100MB storage, 1,000 req/day
- Pro tier: $5/month for unlimited henvs, 10GB storage, 100k req/day
- Custom domains: +$2/month per domain

---

## Disaster Recovery

### Backup Strategy
- **Continuous replication**: Litestream to S3 (sub-second lag)
- **Retention**: 7 days point-in-time recovery
- **Full backup**: Weekly snapshot of all henvs

### Recovery Procedures

**Single henv corruption:**
```bash
# Restore from Litestream
litestream restore -o /data/henvs/{hash}.db s3://hbf-backups/henvs/{hash}.db
```

**Cluster failure:**
1. Deploy new k3s cluster
2. Install HBF + dependencies
3. Restore all henvs from S3
4. Update Cloudflare Tunnel endpoint
5. Downtime: ~15-30 minutes

**Data center loss:**
- S3 replication to multiple regions (if critical)
- DNS failover to backup cluster (if redundant deployment)

---

## Deployment Checklist

### Initial Setup
- [ ] Provision k3s cluster (or single node)
- [ ] Install Traefik ingress controller
- [ ] Configure Cloudflare Tunnel
- [ ] Set up Let's Encrypt ACME for `*.ipsaw.com`
- [ ] Deploy HBF with persistent storage
- [ ] Configure Litestream replication
- [ ] Set up monitoring (Prometheus/Grafana)
- [ ] Configure log aggregation
- [ ] Create inactivity cleanup cron job
- [ ] Test `/new` endpoint and henv creation
- [ ] Verify custom domain CNAME routing

### Ongoing Operations
- [ ] Monitor disk usage (alert at 80%)
- [ ] Review inactive henv cleanup logs
- [ ] Check S3 backup integrity weekly
- [ ] Update HBF binary on security patches
- [ ] Renew Let's Encrypt certificates (auto)
- [ ] Scale resources as henv count grows

---

## Future Enhancements

### Performance
- HTTP/2 and HTTP/3 support (via Traefik)
- SQLite read replicas for heavy read workloads
- CDN integration for static assets
- QuickJS bytecode caching

### Features
- WebSocket clustering (sticky sessions + shared state)
- GraphQL support (QuickJS endpoint)
- Scheduled jobs (cron-like triggers)
- Real-time collaboration (CRDT or OT)

### Operations
- Auto-scaling based on CPU/memory metrics
- Geographic distribution (multi-region)
- Zero-downtime deployments (rolling updates)
- A/B testing for HBF versions

---

## Conclusion

HBF's infrastructure design prioritizes:
1. **Simplicity**: Single static binary, no dependencies
2. **Flexibility**: Local dev, self-hosted, or managed service
3. **Isolation**: Per-henv SQLite databases
4. **Scalability**: Horizontal scaling via consistent hashing
5. **Resilience**: Continuous backups via Litestream
6. **Developer experience**: Live in-browser development

The architecture supports starting small (single node, dozens of henvs) and scaling to thousands of henvs across multiple nodes while maintaining operational simplicity.
