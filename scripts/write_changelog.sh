#!/usr/bin/env bash
# write_changelog.sh
# Generate a CHANGELOG from git commit subjects based on tag ranges.
# Usage patterns:
#   - No args: from latest v* tag to HEAD (or all commits if no tag exists)
#   - One arg: if tag/ref exists, from previous v* tag to that ref; otherwise to HEAD
#   - Two args: explicit range between OLD_TAG and NEW_TAG (both must exist)
# Use -o/--output to write to a different file (default: CHANGELOG.md).

set -euo pipefail

out_file="CHANGELOG.md"

usage() {
  echo "Usage:"
  echo "  $0 [-o <outfile>]                          # Commits since last tag to HEAD"
  echo "  $0 [-o <outfile>] <NEW_TAG>                # If NEW_TAG exists: commits since previous tag to NEW_TAG;"
  echo "                                             # if NEW_TAG does NOT exist: commits since last tag to HEAD"
  echo "  $0 [-o <outfile>] <OLD_TAG> <NEW_TAG>      # Commits between OLD_TAG..NEW_TAG (both must exist)"
}

check_ref_exists() {
  local ref="$1"
  git rev-parse -q --verify "${ref}^{commit}" >/dev/null 2>&1
}

check_tag_exists() {
  local tag="$1"
  git rev-parse "$tag" >/dev/null 2>&1 || { echo "Error: Tag '$tag' not found." >&2; exit 1; }
}

ensure_order_old_before_new() {
  local old="$1" new="$2"
  git merge-base --is-ancestor "$old" "$new" || {
    echo "Error: '$new' is not after '$old'. Provide the older tag first." >&2; exit 1;
  }
}

write_log() {
  local title="$1"
  local range_arg="$2"  # 'A..B', 'TAG', 'HEAD' or 'ALL'
  local log
  if [[ "$range_arg" == "ALL" ]]; then
    log=$(git log --pretty=format:"- %s")
  else
    log=$(git log "$range_arg" --pretty=format:"- %s")
  fi
  [[ -n "${log}" ]] || log="(no changes)"
  { echo "# ${title}"; echo; echo "${log}"; } > "${out_file}"
  echo "Wrote ${out_file}"
}

# ---- Parse options ----
declare -a pos=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -o|--output)
      [[ $# -ge 2 ]] || { echo "Error: -o requires a filename." >&2; usage; exit 1; }
      out_file="$2"; shift 2 ;;
    -h|--help)
      usage; exit 0 ;;
    --) shift; while [[ $# -gt 0 ]]; do pos+=("$1"); shift; done; break ;;
    -*) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    *)  pos+=("$1"); shift ;;
  esac
done

# ---- Main logic ----
case "${#pos[@]}" in
  0)
    last_tag=$(git describe --tags --abbrev=0 --match "v*" 2>/dev/null || true)
    if [[ -z "${last_tag}" ]]; then
      write_log "Changelog (up to HEAD, first release)" "ALL"
    else
      write_log "Changelog from ${last_tag} to HEAD" "${last_tag}..HEAD"
    fi
    ;;
  1)
    requested="${pos[0]}"
    if check_ref_exists "$requested"; then
      new_ref="$requested"
    else
      # requested tag/ref does not exist -> up to HEAD
      new_ref="HEAD"
    fi

    # Find previous tag relative to new_ref (for HEAD = latest tag)
    prev_tag=$(git describe --tags --abbrev=0 --match "v*" "${new_ref}^" 2>/dev/null || true)

    if [[ -z "${prev_tag}" ]]; then
      # First release up to new_ref (HEAD or existing tag)
      write_log "Changelog (up to ${new_ref}, first release)" "${new_ref}"
    else
      ensure_order_old_before_new "${prev_tag}" "${new_ref}"
      if [[ "${new_ref}" == "HEAD" ]]; then
        write_log "Changelog from ${prev_tag} to HEAD" "${prev_tag}..HEAD"
      else
        write_log "Changelog from ${prev_tag} to ${new_ref}" "${prev_tag}..${new_ref}"
      fi
    fi
    ;;
  2)
    old_tag="${pos[0]}"; new_tag="${pos[1]}"
    check_tag_exists "$old_tag"; check_tag_exists "$new_tag"
    ensure_order_old_before_new "$old_tag" "$new_tag"
    write_log "Changelog from ${old_tag} to ${new_tag}" "${old_tag}..${new_tag}"
    ;;
  *)
    usage; exit 1 ;;
esac
