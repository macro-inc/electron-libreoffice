#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"

cd "$BASE_PATH"

# Remove all unused branches and aggressively prune objects that aren't used, shaves off about 25 GiB
find .git-cache -maxdepth 1 -type d -print0 | while read -d $'\0' GC_TARGET
do
  cd -- "$BASE_PATH/$GC_TARGET" || exit 0
  CUR_BRANCH="$(git branch --show-current)"
  # Only shrink attached HEAD
  if [ "x$CUR_BRANCH" != "x" ]; then
    git config remote.origin.fetch "+refs/heads/$CUR_BRANCH:refs/heads/$CUR_BRANCH"
    git config --add remote.origin.fetch "+refs/heads/main:refs/heads/main"
    git branch | grep -v master$ | grep -v main$ | grep -v ^* | xargs git branch -D
    git gc --aggressive --prune=all
  fi
done
