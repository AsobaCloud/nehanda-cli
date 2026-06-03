#!/bin/sh
# git-askpass-forge.sh — the GIT_ASKPASS shim the forge-credential broker
# installs (workspace-resource-plane §4). git invokes GIT_ASKPASS with the
# username/password prompt text and uses the program's stdout as the answer.
#
# The brokered short-lived forge token is passed to the child process in the
# GH_TOKEN environment variable (see forge_cred_build_env), NEVER on the command
# line or on disk. This shim echoes it as the password (and, for the username
# prompt, the conventional "x-access-token", which token-auth forges accept; the
# repo URL may also carry the username). Nothing is logged.
case "$1" in
*[Uu]sername*) echo "x-access-token" ;;
*) printf '%s\n' "$GH_TOKEN" ;;
esac
