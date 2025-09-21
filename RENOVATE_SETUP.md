# GitHub Token Setup for Renovate

## Overview

The Renovate workflow requires a GitHub token to create pull requests and manage dependencies. There are two options for configuration:

## Option 1: Use Default GitHub Token (Temporary/Testing)

**Current Status**: ✅ **Already Configured**

The workflow is now configured to fall back to the default `GITHUB_TOKEN` if no custom `RENOVATE_TOKEN` is provided. This works for basic functionality but has some limitations:

- Pull requests will be created by the GitHub Actions bot
- Some advanced Renovate features may be limited
- Suitable for testing and initial setup

## Option 2: Create Custom RENOVATE_TOKEN (Recommended for Production)

For full functionality, create a GitHub Personal Access Token:

### Step 1: Create GitHub Personal Access Token

1. **Go to GitHub Settings**:
   - Visit: https://github.com/settings/personal-access-tokens/new
   - Or navigate: GitHub Settings → Developer settings → Personal access tokens → Tokens (classic)

2. **Configure Token**:
   - **Note**: "HBF Renovate Token"
   - **Expiration**: Choose appropriate duration (90 days recommended for demo)
   - **Scopes**: Check the following permissions:
     - ✅ `repo` (Full control of private repositories)
     - ✅ `workflow` (Update GitHub Action workflows)
     - ✅ `write:packages` (Upload packages to GitHub Package Registry)

3. **Generate Token**:
   - Click "Generate token"
   - **IMPORTANT**: Copy the token immediately (you won't see it again)

### Step 2: Add Token to Repository Secrets

1. **Navigate to Repository Settings**:
   - Go to: `https://github.com/thetanil/hbf/settings/secrets/actions`
   - Or: Repository → Settings → Secrets and variables → Actions

2. **Create New Secret**:
   - Click "New repository secret"
   - **Name**: `RENOVATE_TOKEN`
   - **Value**: Paste the token from Step 1
   - Click "Add secret"

### Step 3: Test Configuration

Once the token is added, you can test Renovate:

```bash
# Trigger Renovate manually with debug logging
gh workflow run renovate.yml --field logLevel=debug
```

## Current Workflow Status

```yaml
# The workflow now uses fallback logic:
env:
  RENOVATE_TOKEN: ${{ secrets.RENOVATE_TOKEN || secrets.GITHUB_TOKEN }}

# This means:
# - If RENOVATE_TOKEN secret exists → use it (full functionality)
# - If RENOVATE_TOKEN secret missing → use GITHUB_TOKEN (basic functionality)
```

## Expected Renovate Behavior

Once properly configured, Renovate will:

1. **Scan MODULE.bazel** for dependency updates
2. **Create Pull Requests** for:
   - HBF demo components (`hbf_examplecpp`, `hbf_examplepy`)
   - Bazel rules updates (weekly schedule)
3. **Add Labels**: `dependencies`, `hbf-component`, etc.
4. **Create Dashboard**: Track all dependency status
5. **Security Alerts**: Monitor for vulnerabilities

## Troubleshooting

### "You must configure a GitHub token"
- **Solution**: Add `RENOVATE_TOKEN` secret as described above
- **Temporary**: The workflow now falls back to `GITHUB_TOKEN`

### Permission Errors
- **Check**: Token has `repo` and `workflow` scopes
- **Verify**: Token hasn't expired

### No Pull Requests Created
- **Reason**: May be because component repositories don't exist yet
- **Expected**: Will work once `hbf-examplecpp` and `hbf-examplepy` are implemented

## Security Notes

- **Never commit tokens** to the repository
- **Use repository secrets** for secure token storage
- **Set appropriate expiration** for tokens
- **Review token permissions** regularly

---

**Status**: Ready for testing with fallback token configuration ✅
