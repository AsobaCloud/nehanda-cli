#!/usr/bin/env bash
#
# gen-sdks.sh — regenerate the day-one aimee-kb /v1 client SDKs from the
# committed OpenAPI contract (api/openapi-v1.yaml).
#
# Per docs/proposals/accepted/aimee-kb-service-and-public-api.md: the SDKs are
# *generated*, never hand-written. The OpenAPI spec is the source of truth;
# this script is the only sanctioned way to (re)produce api/sdks/<lang>/.
#
# Day-one languages: c, cpp, csharp, go, java, python, rust, typescript.
#
# The script is self-bootstrapping and needs no root:
#   * It uses the system `java` if one is on PATH, otherwise it downloads a
#     portable Temurin JRE into a user cache directory.
#   * It downloads the pinned openapi-generator-cli jar into the same cache.
# Both downloads are one-time and cached across runs.
#
# Usage:
#   scripts/gen-sdks.sh [lang ...]      # regenerate all (default) or a subset
#   AIMEE_SDKGEN_CACHE=/path scripts/gen-sdks.sh
#   OPENAPI_GENERATOR_VERSION=7.22.0 scripts/gen-sdks.sh
#
# Exit non-zero on any generation failure.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Relative spec path: openapi-generator records the -i argument verbatim into
# some clients' README (e.g. csharp `inputSpec:`). Passing a relative path and
# invoking from REPO_ROOT keeps the generated output free of host-specific
# absolute paths, so the tree is identical on every machine.
SPEC_REL="api/openapi-v1.yaml"
SPEC="$REPO_ROOT/$SPEC_REL"
SDK_ROOT="$REPO_ROOT/api/sdks"
CACHE="${AIMEE_SDKGEN_CACHE:-$HOME/.cache/aimee-sdkgen}"
# Pin a known-good generator; override with OPENAPI_GENERATOR_VERSION.
GEN_VERSION="${OPENAPI_GENERATOR_VERSION:-7.22.0}"
TEMURIN_FEATURE="${AIMEE_SDKGEN_JDK:-21}"

log()  { printf '\033[1;34m[gen-sdks]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[gen-sdks] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

[ -f "$SPEC" ] || die "OpenAPI spec not found: $SPEC"
command -v curl >/dev/null 2>&1 || die "curl is required"
command -v tar  >/dev/null 2>&1 || die "tar is required"

mkdir -p "$CACHE" "$SDK_ROOT"

# --- Bootstrap a Java runtime -------------------------------------------------
resolve_java() {
   if command -v java >/dev/null 2>&1; then
      echo "java"
      return 0
   fi
   local jre_java="$CACHE/jre/bin/java"
   if [ -x "$jre_java" ]; then
      echo "$jre_java"
      return 0
   fi
   log "no system Java; downloading portable Temurin ${TEMURIN_FEATURE} JRE"
   local url="https://api.adoptium.net/v3/binary/latest/${TEMURIN_FEATURE}/ga/linux/x64/jre/hotspot/normal/eclipse?project=jdk"
   curl -fsSL -m 600 -o "$CACHE/jre.tar.gz" "$url" \
      || die "failed to download Temurin JRE from $url"
   mkdir -p "$CACHE/jre"
   tar -xzf "$CACHE/jre.tar.gz" -C "$CACHE/jre" --strip-components=1 \
      || die "failed to extract Temurin JRE"
   [ -x "$jre_java" ] || die "extracted JRE missing java binary"
   echo "$jre_java"
}

# --- Bootstrap the openapi-generator jar -------------------------------------
resolve_generator_jar() {
   local jar="$CACHE/openapi-generator-cli-${GEN_VERSION}.jar"
   if [ ! -f "$jar" ]; then
      log "downloading openapi-generator-cli ${GEN_VERSION}"
      local base="https://repo1.maven.org/maven2/org/openapitools/openapi-generator-cli/${GEN_VERSION}"
      curl -fsSL -m 600 -o "$jar" "$base/openapi-generator-cli-${GEN_VERSION}.jar" \
         || die "failed to download openapi-generator-cli ${GEN_VERSION}"
   fi
   echo "$jar"
}

JAVA="$(resolve_java)"
JAR="$(resolve_generator_jar)"
log "java:      $("$JAVA" -version 2>&1 | head -1)"
log "generator: $("$JAVA" -jar "$JAR" version 2>/dev/null || echo "$GEN_VERSION")"

# --- Language matrix ----------------------------------------------------------
# Each entry: <lang> <openapi-generator-name> <extra additional-properties>
#
# Names chosen for dependency-light, widely-used clients:
#   typescript -> typescript-fetch (no axios dependency, browser + node)
#   cpp        -> cpp-restsdk (Microsoft cpprestsdk client)
gen_one() {
   local lang="$1" gname="$2" extra="$3"
   local out="$SDK_ROOT/$lang"
   log "generating $lang ($gname) -> api/sdks/$lang"
   rm -rf "$out"
   mkdir -p "$out"
   # hideGenerationTimestamp drops the embedded build date (java/csharp/go etc.)
   # so regeneration from an unchanged spec is a byte-for-byte no-op. Unknown
   # additional-properties are ignored by generators that don't support it.
   # shellcheck disable=SC2086
   ( cd "$REPO_ROOT" && "$JAVA" -jar "$JAR" generate \
      -g "$gname" \
      -i "$SPEC_REL" \
      -o "$out" \
      --global-property=apiTests=true,modelTests=true,apiDocs=true,modelDocs=true \
      --additional-properties=hideGenerationTimestamp=true,$extra ) \
      >"$out/.gen.log" 2>&1 \
      || { tail -30 "$out/.gen.log" >&2; die "generation failed for $lang (see $out/.gen.log)"; }
   rm -f "$out/.gen.log"
   # The generator stamps a host-specific absolute path / version into this
   # bookkeeping dir; drop it so regeneration is reproducible across machines.
   rm -rf "$out/.openapi-generator"
   rm -f "$out/.openapi-generator-ignore"
   # Don't commit binary build-tool wrappers (e.g. gradle-wrapper.jar). They're
   # not part of the API client and binaries trip supply-chain scanners; users
   # regenerate them with their own toolchain. Text wrappers (gradlew) stay.
   find "$out" -name '*.jar' -delete 2>/dev/null || true
}

declare -A GNAME EXTRA
GNAME[c]="c";                  EXTRA[c]="projectName=aimee-kb-client,packageName=aimee_kb"
GNAME[cpp]="cpp-restsdk";      EXTRA[cpp]="packageName=aimee_kb,packageVersion=1.0.0"
# packageGuid pinned: the csharp generator otherwise mints a random GUID per
# run (in .sln + README), which would defeat the no-op-on-regen guarantee.
GNAME[csharp]="csharp";        EXTRA[csharp]="packageName=AimeeKb,packageVersion=1.0.0,targetFramework=net8.0,packageGuid={E3707CF5-8650-48BB-8F70-3B71551E7760}"
GNAME[go]="go";                EXTRA[go]="packageName=aimeekb,isGoSubmodule=true,withGoMod=true"
GNAME[java]="java";            EXTRA[java]="groupId=ai.aimee,artifactId=aimee-kb-client,artifactVersion=1.0.0,library=native"
GNAME[python]="python";        EXTRA[python]="projectName=aimee-kb-client,packageName=aimee_kb,packageVersion=1.0.0"
GNAME[rust]="rust";            EXTRA[rust]="packageName=aimee-kb-client,packageVersion=1.0.0"
GNAME[typescript]="typescript-fetch"; EXTRA[typescript]="npmName=@aimee/kb-client,npmVersion=1.0.0,supportsES6=true"

ALL_LANGS=(c cpp csharp go java python rust typescript)
if [ "$#" -gt 0 ]; then
   LANGS=("$@")
else
   LANGS=("${ALL_LANGS[@]}")
fi

for lang in "${LANGS[@]}"; do
   [ -n "${GNAME[$lang]:-}" ] || die "unknown SDK language: $lang (known: ${ALL_LANGS[*]})"
   gen_one "$lang" "${GNAME[$lang]}" "${EXTRA[$lang]}"
done

log "done. SDKs written under api/sdks/{$(IFS=,; echo "${LANGS[*]}")}"
log "verify coverage with: python3 scripts/check-sdk-parity.py"
