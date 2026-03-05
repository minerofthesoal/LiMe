#!/bin/bash
# Modern LiMe ISO builder orchestrator
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

log() { echo "[lime-build] $*"; }

usage() {
  cat <<USAGE
Usage: $0 [--sync-upstream] [--repo-url URL] [--repo-branch BRANCH] [--skip-iso]
USAGE
}

SYNC_UPSTREAM=""
REPO_URL=""
REPO_BRANCH="main"
SKIP_ISO=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sync-upstream) SYNC_UPSTREAM="--sync-upstream"; shift ;;
    --repo-url) REPO_URL="$2"; shift 2 ;;
    --repo-branch) REPO_BRANCH="$2"; shift 2 ;;
    --skip-iso) SKIP_ISO="--skip-iso"; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1"; usage; exit 1 ;;
  esac
done

CMD=(python3 "$PROJECT_ROOT/build.py" --clean)
[[ -n "$SYNC_UPSTREAM" ]] && CMD+=("$SYNC_UPSTREAM")
[[ -n "$SKIP_ISO" ]] && CMD+=("$SKIP_ISO")
[[ -n "$REPO_URL" ]] && CMD+=(--repo-url "$REPO_URL")
CMD+=(--repo-branch "$REPO_BRANCH")

log "Running: ${CMD[*]}"
"${CMD[@]}"

log "Build completed. See out/BUILD_REPORT.txt"
