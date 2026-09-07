#!/usr/bin/env bash
# Mechanical git-hygiene check for the Personal Development Branch Rule
# (AGENTS.md). Run this instead of trusting memory of the rule -- it
# reports the actual state, doesn't rely on anyone remembering to check.
#
# Usage: scripts/agent-git-check.sh [agent-prefix] [repo-dir ...]
#   agent-prefix defaults to "claude"; repo-dir defaults to "." (the repo
#   this script is invoked from). Pass one or more repo paths to check
#   several at once, e.g.:
#     scripts/agent-git-check.sh claude . apps/CreationEngine
#
# Exit code is nonzero if ANY checked repo has a real problem (wrong
# branch, unsynced dev branch, dirty tree, stray branches) -- safe to
# wire into a session-start routine and treat a nonzero exit as "stop
# and look before doing anything else."

set -u
AGENT="${1:-claude}"
shift || true
REPOS=("${@:-.}")

overall_status=0
RED=$'\033[31m'; YELLOW=$'\033[33m'; GREEN=$'\033[32m'; RESET=$'\033[0m'

check_repo() {
    local repo="$1"
    echo "== ${repo} =="
    if ! git -C "$repo" rev-parse --git-dir > /dev/null 2>&1; then
        echo "${RED}not a git repo -- skipping${RESET}"
        overall_status=1
        return
    fi

    local branch
    branch=$(git -C "$repo" rev-parse --abbrev-ref HEAD 2>/dev/null)
    local dev_branch="${AGENT}/development"
    local repo_problem=0

    # 1. Branch identity: on our own dev branch, or a short-lived task
    #    branch that also carries our prefix? Anything else is a red flag.
    if [[ "$branch" == "$dev_branch" ]]; then
        echo "branch: ${GREEN}${branch}${RESET} (on the standing dev branch)"
    elif [[ "$branch" == "${AGENT}/"* ]]; then
        echo "branch: ${YELLOW}${branch}${RESET} (a task branch, not the standing dev branch -- fine if short-lived and merging back into ${dev_branch} soon)"
    elif [[ "$branch" == "master" || "$branch" == "main" ]]; then
        echo "branch: ${RED}${branch}${RESET} -- checked out directly on the protected branch. Do not commit here."
        repo_problem=1
    else
        echo "branch: ${RED}${branch}${RESET} -- does not match '${AGENT}/*' at all. Confirm this isn't another agent's branch (Branch Ownership Check, AGENTS.md) before touching it."
        repo_problem=1
    fi

    # 2. Working tree cleanliness.
    local dirty
    dirty=$(git -C "$repo" status --porcelain 2>/dev/null)
    if [[ -n "$dirty" ]]; then
        local dirty_count
        dirty_count=$(echo "$dirty" | wc -l | tr -d ' ')
        echo "tree: ${YELLOW}${dirty_count} uncommitted change(s)${RESET} -- confirm these are yours and intended before starting anything new:"
        echo "$dirty" | sed 's/^/  /'
    else
        echo "tree: ${GREEN}clean${RESET}"
    fi

    # 3. Sync status vs origin/master, IF the dev branch exists locally.
    if git -C "$repo" show-ref --verify --quiet "refs/heads/${dev_branch}"; then
        git -C "$repo" fetch origin master --quiet 2>/dev/null
        if git -C "$repo" rev-parse --verify --quiet origin/master > /dev/null; then
            local ahead behind
            ahead=$(git -C "$repo" rev-list --count "origin/master..${dev_branch}" 2>/dev/null || echo "?")
            behind=$(git -C "$repo" rev-list --count "${dev_branch}..origin/master" 2>/dev/null || echo "?")
            if [[ "$behind" != "0" ]]; then
                echo "${dev_branch} vs origin/master: ${RED}behind by ${behind}${RESET} -- sync NOW (fetch + fast-forward) before doing anything else. This is the exact gap that caused this week's branch sprawl."
                repo_problem=1
            elif [[ "$ahead" == "0" ]]; then
                echo "${dev_branch} vs origin/master: ${GREEN}in sync${RESET}"
            else
                echo "${dev_branch} vs origin/master: ${GREEN}ahead by ${ahead}${RESET} (your own unmerged work, expected)"
            fi
        fi
    else
        echo "${YELLOW}no local ${dev_branch} branch yet in this repo${RESET} -- create one off origin/master before starting task work here."
    fi

    # 4. Stray local branches bearing our prefix that aren't the dev branch
    #    and look stale (fully merged into origin/master already).
    local stray
    stray=$(git -C "$repo" for-each-ref --format='%(refname:short)' "refs/heads/${AGENT}/*" 2>/dev/null | grep -v "^${dev_branch}$")
    if [[ -n "$stray" ]]; then
        echo "other ${AGENT}/* branches present:"
        while IFS= read -r b; do
            [[ -z "$b" ]] && continue
            if git -C "$repo" merge-base --is-ancestor "$b" origin/master 2>/dev/null; then
                echo "  ${YELLOW}${b}${RESET} -- fully merged into origin/master already, safe to delete (git branch -d ${b})"
            else
                echo "  ${b} -- has unmerged commits, leave it unless you know it's done"
            fi
        done <<< "$stray"
    fi

    if [[ $repo_problem -ne 0 ]]; then
        overall_status=1
    fi
    echo
}

for repo in "${REPOS[@]}"; do
    check_repo "$repo"
done

if [[ $overall_status -ne 0 ]]; then
    echo "${RED}One or more repos need attention before proceeding.${RESET}"
else
    echo "${GREEN}All checked repos are straight.${RESET}"
fi
exit $overall_status
