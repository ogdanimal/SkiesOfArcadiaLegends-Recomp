#!/usr/bin/env bash
# Cut a release from a commit on dev that validate passed on: fast-forward main to it, tag it (which
# starts release.yml), wait for the GitHub Release, then replace its generated text with the notes.
#   tools/release.sh VERSION SHA NOTES.md      e.g. tools/release.sh 0.9.0-beta 1a2b3c4 notes-0.9.0.md
# Release notes say what this release adds or fixes, and link the README for the rest.
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION=${1:?usage: tools/release.sh VERSION SHA NOTES.md}
SHA=$(git rev-parse --verify "${2:?usage: tools/release.sh VERSION SHA NOTES.md}^{commit}")
NOTES=$(realpath "${3:?usage: tools/release.sh VERSION SHA NOTES.md}")
TAG=v$VERSION
REPO=$(gh repo view --json nameWithOwner --jq .nameWithOwner)
[[ $TAG =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?$ ]] || { echo "$TAG is not vMAJOR.MINOR.PATCH[-suffix]"; exit 1; }
[ -s "$NOTES" ] || { echo "$NOTES is empty"; exit 1; }

git fetch -q origin dev main
git merge-base --is-ancestor "$SHA" origin/dev || { echo "$SHA is not on origin/dev -- stopping."; exit 1; }
git merge-base --is-ancestor origin/main "$SHA" || { echo "main is not behind $SHA -- stopping."; exit 1; }
git ls-remote --exit-code origin "refs/tags/$TAG" >/dev/null && { echo "$TAG already exists -- stopping."; exit 1; }
RUN=$(gh run list --repo "$REPO" --workflow validate.yml --commit "$SHA" --json conclusion --jq '[.[] | select(.conclusion == "success")] | length')
[ "$RUN" -gt 0 ] || { echo "no green validate run for $SHA -- stopping."; exit 1; }

echo "1/3 fast-forwarding main to $SHA"
git push origin "$SHA:refs/heads/main"
echo "2/3 tagging $TAG (starts release.yml)"
git tag "$TAG" "$SHA"
git push origin "$TAG"

# On the private repo, pushes have been recorded without starting workflows (2026-10-13): if no
# release run shows up for the tag, start it by hand on the tag
for _ in 1 2 3 4 5 6; do
  [ -n "$(gh run list --repo "$REPO" --workflow release.yml --branch "$TAG" --json databaseId --jq '.[0].databaseId')" ] && break
  sleep 20
done
if [ -z "$(gh run list --repo "$REPO" --workflow release.yml --branch "$TAG" --json databaseId --jq '.[0].databaseId')" ]; then
  echo "   no run started by the tag push; starting release.yml on $TAG by hand"
  gh workflow run release.yml --repo "$REPO" --ref "$TAG"
fi

echo "3/3 waiting for the release workflow to publish (checks every minute)"
until gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; do sleep 60; printf .; done; echo
gh release edit "$TAG" --repo "$REPO" --notes-file "$NOTES"
gh release view "$TAG" --repo "$REPO" --json url,assets --jq '.url, (.assets[] | "  \(.name)")'
echo "Done."
