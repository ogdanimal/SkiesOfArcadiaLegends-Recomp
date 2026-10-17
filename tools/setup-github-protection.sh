#!/usr/bin/env bash
# Code review B7 (docs/handoffs/HANDOFF-review.md): keep the secrets (the secrets repo's token, the
# artifact key, the release signing key) away from any ref but dev, main and release tags, and make
# release tags an admin's act. Needs Environments and rulesets, which GitHub's Free plan only has for
# PUBLIC repos, so this runs once the repo is public. Idempotent: run it again to re-assert.
#   tools/setup-github-protection.sh OWNER/REPO
# Creates:
#   environment ci       deployments from the branches dev and main and v* tags (the build jobs:
#                        the secrets repo's token, the artifact key)
#   environment release  deployments from v* tags only, approved by the repo owner (the android-sign
#                        job alone: the release signing key)
#   ruleset "release tags"  only admins create, move or delete v* tags
#   ruleset "main"          main can't be deleted or force-pushed
#   Actions                 only GitHub's own actions and the three third-party ones the workflows
#                           use, each pinned to a full commit SHA
#   security features       secret scanning with push protection, Dependabot alerts, private
#                           vulnerability reporting
#   old CI outputs          every artifact and cache made before this (they were encrypted with, or
#                           keyed on, the artifact key the next step replaces)
# Then: tools/set-release-secrets.sh --environments (moves the secrets into the environments and
# deletes the repo-level copies), then the environment: lines in validate.yml: `environment: ci` on
# codegen, windows, linux and android-module (the jobs that read secrets; android runs Gradle and gets
# none), `environment: release` on android-sign.
set -euo pipefail
REPO=${1:?usage: tools/setup-github-protection.sh OWNER/REPO}
[ "$(gh api "repos/$REPO" --jq .visibility)" = public ] ||
  { echo "$REPO isn't public: Environments and rulesets need a public repo on the Free plan."; exit 1; }
OWNER_ID=$(gh api "users/${REPO%%/*}" --jq .id)

env_with() {  # env_with NAME TYPE:PATTERN...  (TYPE branch or tag)
  local name=$1; shift
  local body='{"deployment_branch_policy":{"protected_branches":false,"custom_branch_policies":true}'
  [ "$name" = release ] && body+=",\"reviewers\":[{\"type\":\"User\",\"id\":$OWNER_ID}],\"prevent_self_review\":false"
  body+='}'
  gh api -X PUT "repos/$REPO/environments/$name" --input - <<< "$body" >/dev/null
  # Replace the deployment policies with exactly these
  for id in $(gh api "repos/$REPO/environments/$name/deployment-branch-policies" --jq '.branch_policies[].id'); do
    gh api -X DELETE "repos/$REPO/environments/$name/deployment-branch-policies/$id" >/dev/null
  done
  for p in "$@"; do
    gh api -X POST "repos/$REPO/environments/$name/deployment-branch-policies" \
      -f name="${p#*:}" -f type="${p%%:*}" >/dev/null
  done
  echo "environment $name: $*"
}
env_with ci branch:dev branch:main 'tag:v*'
env_with release 'tag:v*'

ruleset() {  # ruleset NAME JSON
  local id
  id=$(gh api "repos/$REPO/rulesets" --jq ".[] | select(.name == \"$1\") | .id")
  if [ -n "$id" ]; then
    gh api -X PUT "repos/$REPO/rulesets/$id" --input - <<< "$2" >/dev/null
  else
    gh api -X POST "repos/$REPO/rulesets" --input - <<< "$2" >/dev/null
  fi
  echo "ruleset $1"
}
# Repository role 5 = admin
ruleset "release tags" '{"name":"release tags","target":"tag","enforcement":"active",
  "conditions":{"ref_name":{"include":["refs/tags/v*"],"exclude":[]}},
  "bypass_actors":[{"actor_id":5,"actor_type":"RepositoryRole","bypass_mode":"always"}],
  "rules":[{"type":"creation"},{"type":"update"},{"type":"deletion"}]}'
ruleset "main" '{"name":"main","target":"branch","enforcement":"active",
  "conditions":{"ref_name":{"include":["refs/heads/main"],"exclude":[]}},
  "rules":[{"type":"deletion"},{"type":"non_fast_forward"}]}'

gh api "repos/$REPO/environments" --jq '.environments[] | "  \(.name): \([.protection_rules[]?.type] | join(", "))"'

# Actions: GitHub's own plus exactly the third-party actions in .github/workflows, SHA-pinned
gh api -X PUT "repos/$REPO/actions/permissions" --input - >/dev/null <<< \
  '{"enabled":true,"allowed_actions":"selected","sha_pinning_required":true}'
gh api -X PUT "repos/$REPO/actions/permissions/selected-actions" --input - >/dev/null <<< \
  '{"github_owned_allowed":true,"verified_allowed":false,
    "patterns_allowed":["ilammy/msvc-dev-cmd@*","gradle/actions/wrapper-validation@*","gradle/actions@*","android-actions/setup-android@*"]}'
gh api "repos/$REPO/actions/permissions" --jq '"actions: \(.allowed_actions), SHA pinning required: \(.sha_pinning_required)"'

# Free for public repos
gh api -X PATCH "repos/$REPO" --input - >/dev/null <<< \
  '{"security_and_analysis":{"secret_scanning":{"status":"enabled"},"secret_scanning_push_protection":{"status":"enabled"}}}'
gh api -X PUT "repos/$REPO/vulnerability-alerts" >/dev/null
gh api -X PUT "repos/$REPO/private-vulnerability-reporting" >/dev/null
gh api "repos/$REPO" --jq '"secret scanning: \(.security_and_analysis.secret_scanning.status), push protection: \(.security_and_analysis.secret_scanning_push_protection.status)"'

# Old CI outputs: anyone can download a public repo's artifacts; the codegen ones are encrypted with
# the artifact key that set-release-secrets.sh --environments replaces, and nothing needs the old ones
n=0
for id in $(gh api --paginate "repos/$REPO/actions/artifacts" --jq '.artifacts[].id'); do
  gh api -X DELETE "repos/$REPO/actions/artifacts/$id" >/dev/null && n=$((n + 1))
done
for id in $(gh api --paginate "repos/$REPO/actions/caches" --jq '.actions_caches[].id'); do
  gh api -X DELETE "repos/$REPO/actions/caches/$id" >/dev/null && n=$((n + 1))
done
echo "deleted $n old artifacts and caches"
