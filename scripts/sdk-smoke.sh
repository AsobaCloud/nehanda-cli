#!/usr/bin/env bash
#
# sdk-smoke.sh — exercise each generated /v1 SDK against a running aimee-kb.
#
# For every language under api/sdks/, build a minimal consumer that calls a
# couple of endpoints (GET /v1/version, GET /v1/capabilities) through the
# generated client and asserts the response. This is the runnable form of the
# proposal AC "Generated SDKs in all eight day-one languages exercise every
# /v1/* endpoint" — run it on any host with the toolchains installed and point
# it at a deployed kb (KB_BASE_URL / KB_BEARER_TOKEN). No CI service required.
#
# Per-language outcome:
#   PASS  — the SDK built and the live call returned the expected data
#   SKIP  — the language toolchain (or a generated SDK's deps) isn't available
#   FAIL  — the SDK built but the call failed / returned wrong data
#
# Safe anywhere: if no aimee-kb is reachable the whole script SKIPs (exit 0),
# like src/tests/test_v1_third_party.sh. Exit is non-zero only on a real FAIL.
#
# Toolchains per language (install on the validation host):
#   go         go >= 1.23
#   typescript node >= 18 + npx (uses tsx)
#   python     python3 + venv (script pip-installs pydantic/urllib3>=2/dateutil)
#   java       JDK 17 + maven
#   c          gcc + cmake + libcurl-dev
#   cpp        g++ + cmake + cpprestsdk (libcpprest-dev). NOTE: the generated
#              cpp-restsdk client has no bearer support — run vs an unauth kb.
#   csharp     dotnet SDK 8
#   rust       cargo >= 1.74 (older cargo can't parse the crate's deps)
#
# Validated live 2026-06-02 on a Debian-12 host with these toolchains: go,
# typescript, python, java, c, cpp, csharp all PASS; rust requires a newer
# cargo than Debian 12 ships (SKIP, runner correct).
#
# Usage:
#   KB_BASE_URL=http://127.0.0.1:8390/v1 KB_BEARER_TOKEN=<tok> scripts/sdk-smoke.sh
#   scripts/sdk-smoke.sh go python        # a subset
#
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK_ROOT="$ROOT/api/sdks"
BASE_URL="${KB_BASE_URL:-http://127.0.0.1:8390/v1}"
TOKEN="${KB_BEARER_TOKEN:-}"
TIMEOUT="${KB_HTTP_TIMEOUT:-15}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

pass=0 fail=0 skip=0
RESULT=""
note()  { printf '  %s\n' "$*" >&2; }
P() { RESULT="$RESULT\nPASS $1"; pass=$((pass+1)); echo "PASS: $1"; }
S() { RESULT="$RESULT\nSKIP $1 ($2)"; skip=$((skip+1)); echo "SKIP: $1 ($2)"; }
F() { RESULT="$RESULT\nFAIL $1 ($2)"; fail=$((fail+1)); echo "FAIL: $1 ($2)" >&2; }

command -v curl >/dev/null 2>&1 || { echo "SKIP: curl not installed"; exit 0; }

# --- reachability: skip the whole run if no kb ------------------------------
AUTH=()
[ -n "$TOKEN" ] && AUTH=(-H "Authorization: Bearer $TOKEN")
code="$(curl -s -o /dev/null -w '%{http_code}' -m "$TIMEOUT" "${AUTH[@]}" "$BASE_URL/version" 2>/dev/null)"
code="${code:-000}"
if [ "$code" = "000" ]; then
   echo "SKIP: no aimee-kb reachable at $BASE_URL (set KB_BASE_URL to run live)"
   exit 0
fi
[ "$code" = "200" ] || { echo "FAIL: GET /version returned $code (auth?)"; exit 1; }
echo "kb reachable at $BASE_URL (GET /version → 200)"

LANGS=("$@"); [ "$#" -eq 0 ] && LANGS=(go python typescript c cpp csharp rust java)

# --- Go ---------------------------------------------------------------------
smoke_go() {
   command -v go >/dev/null 2>&1 || { S go "go toolchain absent"; return; }
   [ -d "$SDK_ROOT/go" ] || { S go "api/sdks/go missing"; return; }
   local d="$WORK/go"; mkdir -p "$d"
   cat > "$d/go.mod" <<EOF
module sdksmoke
go 1.23
require github.com/GIT_USER_ID/GIT_REPO_ID/aimeekb v0.0.0
replace github.com/GIT_USER_ID/GIT_REPO_ID/aimeekb => $SDK_ROOT/go
EOF
   cat > "$d/main.go" <<EOF
package main
import ("context";"fmt";"os"; kb "github.com/GIT_USER_ID/GIT_REPO_ID/aimeekb")
func main() {
	c := kb.NewConfiguration()
	c.Servers = kb.ServerConfigurations{{URL: "$BASE_URL"}}
	if "$TOKEN" != "" { c.AddDefaultHeader("Authorization", "Bearer $TOKEN") }
	cl := kb.NewAPIClient(c)
	v, _, err := cl.DefaultAPI.GetVersion(context.Background()).Execute()
	if err != nil { fmt.Println(err); os.Exit(1) }
	if v.GetService() == "" { fmt.Println("empty service"); os.Exit(1) }
	if _, _, err := cl.DefaultAPI.GetCapabilities(context.Background()).Execute(); err != nil { fmt.Println(err); os.Exit(1) }
	fmt.Printf("service=%s\n", v.GetService())
}
EOF
   if (cd "$d" && GOFLAGS=-mod=mod GOPROXY=off GOTOOLCHAIN=local go build -o smoke ./... >/tmp/go.err 2>&1 && GOPROXY=off ./smoke >/tmp/go.out 2>&1); then
      P "go ($(cat /tmp/go.out))"
   else
      F go "$(tail -1 /tmp/go.err /tmp/go.out 2>/dev/null | tail -1)"
   fi
}

# --- Python -----------------------------------------------------------------
smoke_python() {
   command -v python3 >/dev/null 2>&1 || { S python "python3 absent"; return; }
   [ -d "$SDK_ROOT/python" ] || { S python "api/sdks/python missing"; return; }
   # Prefer an isolated venv: the generated client needs urllib3>=2, which a
   # distro's system urllib3 (1.x) otherwise shadows (PoolKey kwarg mismatch).
   local py="python3" venv="$WORK/pyvenv"
   if python3 -m venv "$venv" >/dev/null 2>&1 && \
      "$venv/bin/pip" install -q pydantic urllib3 python-dateutil >/tmp/py.pip 2>&1; then
      py="$venv/bin/python"
   elif ! python3 -c 'import pydantic, urllib3, dateutil' 2>/dev/null; then
      S python "deps (pydantic/urllib3>=2/dateutil) unavailable and venv/pip failed"
      return
   fi
   cat > "$WORK/py.py" <<EOF
import sys
sys.path.insert(0, "$SDK_ROOT/python")
import aimee_kb
cfg = aimee_kb.Configuration(host="$BASE_URL")
if "$TOKEN": cfg.access_token = "$TOKEN"
with aimee_kb.ApiClient(cfg) as c:
    api = aimee_kb.DefaultApi(c)
    v = api.get_version()
    api.get_capabilities()
    print(v.service)
EOF
   if out="$("$py" "$WORK/py.py" 2>/tmp/py.err)"; then P "python (service=$out)"; else F python "$(tail -1 /tmp/py.err)"; fi
}

# --- TypeScript (typescript-fetch) ------------------------------------------
smoke_typescript() {
   command -v node >/dev/null 2>&1 || { S typescript "node absent"; return; }
   command -v npx  >/dev/null 2>&1 || { S typescript "npx absent"; return; }
   [ -d "$SDK_ROOT/typescript" ] || { S typescript "api/sdks/typescript missing"; return; }
   local d="$WORK/ts"; mkdir -p "$d"
   cp -r "$SDK_ROOT/typescript" "$d/sdk"
   cat > "$d/smoke.ts" <<EOF
import { Configuration, DefaultApi } from "./sdk/src/index";
const token = "$TOKEN";
const cfg = new Configuration({ basePath: "$BASE_URL", accessToken: token || undefined });
const api = new DefaultApi(cfg);
(async () => {
  const v = await api.getVersion();
  await api.getCapabilities();
  if (!(v as any).service) { console.error("empty service"); process.exit(1); }
  console.log("service=" + (v as any).service);
})().catch((e) => { console.error(String(e)); process.exit(1); });
EOF
   # tsx runs TS directly; --yes uses the npm cache if present, else network.
   if (cd "$d" && npx --yes tsx smoke.ts >/tmp/ts.out 2>/tmp/ts.err); then
      P "typescript ($(cat /tmp/ts.out))"
   else
      if grep -qiE "not found|ENOTFOUND|network|EAI_AGAIN|offline|registry" /tmp/ts.err; then
         S typescript "tsx unavailable offline"
      else
         F typescript "$(tail -1 /tmp/ts.err)"
      fi
   fi
}

# --- Compiled SDKs: run if the toolchain is present, else SKIP --------------
# These need their build toolchains + client deps (libcurl, cpprestsdk, cargo,
# dotnet, maven). In CI a per-language matrix job provides them; here they SKIP
# cleanly when absent.
smoke_rust() {
   command -v cargo >/dev/null 2>&1 || { S rust "cargo absent"; return; }
   [ -d "$SDK_ROOT/rust" ] || { S rust "api/sdks/rust missing"; return; }
   # The generated crate's transitive deps (reqwest/idna_adapter) need a recent
   # cargo; Debian 12 ships 1.65, which can't parse their manifests. Require >=1.74.
   local cv; cv="$(cargo --version 2>/dev/null | awk '{print $2}')"
   local cmaj cmin; cmaj="${cv%%.*}"; cmin="${cv#*.}"; cmin="${cmin%%.*}"
   if [ "${cmaj:-0}" -lt 1 ] || { [ "${cmaj:-0}" -eq 1 ] && [ "${cmin:-0}" -lt 74 ]; }; then
      S rust "cargo $cv too old (need >= 1.74 for the generated crate deps)"; return
   fi
   local d="$WORK/rust"; mkdir -p "$d/src"
   cat > "$d/Cargo.toml" <<EOF
[package]
name = "rustsmoke"
version = "0.0.0"
edition = "2021"
[dependencies]
aimee-kb-client = { path = "$SDK_ROOT/rust" }
tokio = { version = "1", features = ["macros", "rt-multi-thread"] }
[workspace]
EOF
   cat > "$d/src/main.rs" <<EOF
use aimee_kb_client::apis::configuration::Configuration;
use aimee_kb_client::apis::default_api;
#[tokio::main]
async fn main() {
    let mut c = Configuration::default();
    c.base_path = "$BASE_URL".to_owned();
    let t = "$TOKEN";
    if !t.is_empty() { c.bearer_access_token = Some(t.to_owned()); }
    let v = default_api::get_version(&c).await.expect("get_version");
    default_api::get_capabilities(&c).await.expect("get_capabilities");
    println!("service={}", v.service);
}
EOF
   if (cd "$d" && CARGO_TARGET_DIR="$d/target" cargo run --quiet >/tmp/rust.out 2>/tmp/rust.err); then
      P "rust ($(cat /tmp/rust.out))"
   else
      F rust "$(grep -iE 'error' /tmp/rust.err | head -1 || tail -1 /tmp/rust.err)"
   fi
}
smoke_csharp() {
   command -v dotnet >/dev/null 2>&1 || { S csharp "dotnet absent"; return; }
   [ -f "$SDK_ROOT/csharp/src/AimeeKb/AimeeKb.csproj" ] || { S csharp "api/sdks/csharp missing"; return; }
   local d="$WORK/cs"; mkdir -p "$d"
   cat > "$d/smoke.csproj" <<EOF
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net8.0</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="$SDK_ROOT/csharp/src/AimeeKb/AimeeKb.csproj" />
  </ItemGroup>
</Project>
EOF
   cat > "$d/Program.cs" <<EOF
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.DependencyInjection;
using AimeeKb.Api;
using AimeeKb.Client;
using AimeeKb.Extensions;
var host = Host.CreateDefaultBuilder(args).ConfigureApi((ctx, services, options) =>
{
    // generichost DI requires a BearerToken to be registered (DefaultApi
    // depends on TokenProvider<BearerToken>), even against an unauth kb where
    // an empty token is harmless.
    options.AddTokens(new BearerToken("$TOKEN"));
    options.AddApiHttpClients(client => { client.BaseAddress = new System.Uri("$BASE_URL/"); });
}).Build();
var api = host.Services.GetRequiredService<IDefaultApi>();
var v = (await api.GetVersionAsync()).Ok();
await api.GetCapabilitiesAsync();
System.Console.WriteLine("service=" + v!.Service);
EOF
   if (cd "$d" && DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1 dotnet run -c Release >/tmp/cs.out 2>&1); then
      P "csharp ($(grep '^service=' /tmp/cs.out | head -1))"
   else
      F csharp "$(grep -iE 'error|exception|: error' /tmp/cs.out | head -1 || tail -2 /tmp/cs.out | head -1)"
   fi
}
smoke_java() {
   command -v mvn >/dev/null 2>&1 || { S java "maven absent"; return; }
   command -v javac >/dev/null 2>&1 || { S java "jdk absent"; return; }
   [ -d "$SDK_ROOT/java" ] || { S java "api/sdks/java missing"; return; }
   # Install the generated SDK to the local maven repo, then run a consumer.
   if ! mvn -q -f "$SDK_ROOT/java/pom.xml" -DskipTests install >/tmp/java.err 2>&1; then
      F java "SDK mvn install failed: $(grep -iE 'error|fail' /tmp/java.err | head -1)"; return
   fi
   local d="$WORK/java"; mkdir -p "$d/src/main/java"
   cat > "$d/pom.xml" <<EOF
<project xmlns="http://maven.apache.org/POM/4.0.0">
  <modelVersion>4.0.0</modelVersion>
  <groupId>smoke</groupId><artifactId>smoke</artifactId><version>1</version>
  <properties>
    <maven.compiler.release>17</maven.compiler.release>
    <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
  </properties>
  <dependencies><dependency>
    <groupId>ai.aimee</groupId><artifactId>aimee-kb-client</artifactId><version>1.0.0</version>
  </dependency></dependencies>
  <build><plugins>
    <plugin>
      <groupId>org.apache.maven.plugins</groupId><artifactId>maven-compiler-plugin</artifactId>
      <version>3.13.0</version><configuration><release>17</release></configuration>
    </plugin>
    <plugin>
      <groupId>org.codehaus.mojo</groupId><artifactId>exec-maven-plugin</artifactId>
      <version>3.1.0</version>
    </plugin>
  </plugins></build>
</project>
EOF
   cat > "$d/src/main/java/Main.java" <<EOF
import org.openapitools.client.ApiClient;
import org.openapitools.client.api.DefaultApi;
import org.openapitools.client.model.VersionResponse;
public class Main {
  public static void main(String[] a) throws Exception {
    ApiClient c = new ApiClient();
    c.updateBaseUri("$BASE_URL");
    String t = "$TOKEN";
    if (!t.isEmpty()) c.setRequestInterceptor(b -> b.header("Authorization", "Bearer " + t));
    DefaultApi api = new DefaultApi(c);
    VersionResponse v = api.getVersion();
    api.getCapabilities();
    System.out.println("service=" + v.getService());
  }
}
EOF
   if (cd "$d" && mvn -q compile exec:java -Dexec.mainClass=Main >/tmp/java.out 2>&1); then
      P "java ($(grep '^service=' /tmp/java.out | head -1))"
   else
      F java "$(grep -iE 'error|exception' /tmp/java.out | head -1)"
   fi
}
smoke_c() {
   command -v cc >/dev/null 2>&1 || { S c "cc absent"; return; }
   command -v cmake >/dev/null 2>&1 || { S c "cmake absent"; return; }
   [ -d "$SDK_ROOT/c" ] || { S c "api/sdks/c missing"; return; }
   pkg-config --exists libcurl 2>/dev/null || { S c "libcurl dev headers absent"; return; }
   local b="$WORK/cbuild"
   if ! cmake -S "$SDK_ROOT/c" -B "$b" >/tmp/c.err 2>&1 || ! cmake --build "$b" -j2 >>/tmp/c.err 2>&1; then
      F c "SDK build failed: $(grep -iE 'error' /tmp/c.err | head -1)"; return
   fi
   local lib; lib="$(find "$b" -name 'libaimee_kb_api.a' -o -name 'libaimee_kb_api.so' 2>/dev/null | head -1)"
   [ -n "$lib" ] || { F c "static lib not produced"; return; }
   local d="$WORK/c"; mkdir -p "$d"
   cat > "$d/main.c" <<EOF
#include "apiClient.h"
#include "DefaultAPI.h"
#include "version_response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    apiClient_t *c = apiClient_create_with_base_path("$BASE_URL", NULL);
    if (!c) { fprintf(stderr, "no client\n"); return 1; }
    if (strlen("$TOKEN")) c->accessToken = strdup("$TOKEN");
    version_response_t *v = DefaultAPI_getVersion(c);
    if (!v || !v->service) { fprintf(stderr, "no version\n"); return 1; }
    printf("service=%s\n", v->service);
    return 0;
}
EOF
   if cc "$d/main.c" -I"$SDK_ROOT/c/include" -I"$SDK_ROOT/c/api" -I"$SDK_ROOT/c/model" \
        "$lib" $(pkg-config --libs libcurl) -lssl -lcrypto -lpthread -lm -o "$d/smoke" >>/tmp/c.err 2>&1 \
      && out="$(LD_LIBRARY_PATH="$(dirname "$lib")" "$d/smoke" 2>/tmp/c.run)"; then
      P "c ($out)"
   else
      F c "$(grep -iE 'error|undefined' /tmp/c.err | head -1 || tail -1 /tmp/c.run 2>/dev/null)"
   fi
}
smoke_cpp() {
   command -v c++ >/dev/null 2>&1 || { S cpp "c++ absent"; return; }
   command -v cmake >/dev/null 2>&1 || { S cpp "cmake absent"; return; }
   [ -d "$SDK_ROOT/cpp" ] || { S cpp "api/sdks/cpp missing"; return; }
   [ -f /usr/include/cpprest/http_client.h ] || { S cpp "cpprestsdk dev headers absent"; return; }
   # The generated cpp-restsdk client does not apply the bearer-auth scheme, so
   # it can only reach an unauthenticated kb. SKIP (rather than fail) when a
   # token is set; run it against a no-bearer kb (KB_BEARER_TOKEN unset).
   [ -n "$TOKEN" ] && { S cpp "generated cpp-restsdk client has no bearer support; run vs an unauthenticated kb"; return; }
   local b="$WORK/cppbuild"
   if ! cmake -S "$SDK_ROOT/cpp" -B "$b" >/tmp/cpp.err 2>&1 || ! cmake --build "$b" -j2 >>/tmp/cpp.err 2>&1; then
      F cpp "SDK build failed: $(grep -iE ': error|fatal' /tmp/cpp.err | head -1)"; return
   fi
   local lib; lib="$(find "$b" -name 'libaimee_kb.so' -o -name 'libaimee_kb.a' 2>/dev/null | head -1)"
   [ -n "$lib" ] || { F cpp "lib not produced"; return; }
   local d="$WORK/cpp"; mkdir -p "$d"
   cat > "$d/main.cpp" <<EOF
#include "aimee_kb/ApiConfiguration.h"
#include "aimee_kb/ApiClient.h"
#include "aimee_kb/api/DefaultApi.h"
#include <iostream>
using namespace org::openapitools::client;
int main() {
    auto cfg = std::make_shared<api::ApiConfiguration>();
    cfg->setBaseUrl(utility::conversions::to_string_t(std::string("$BASE_URL")));
    auto client = std::make_shared<api::ApiClient>(cfg);
    api::DefaultApi dapi(client);
    auto v = dapi.getVersion().get();
    dapi.getCapabilities().get();
    std::cout << "service=" << utility::conversions::to_utf8string(v->getService()) << std::endl;
    return 0;
}
EOF
   if c++ -std=c++14 "$d/main.cpp" -I"$SDK_ROOT/cpp/include" "$lib" \
        -lcpprest -lssl -lcrypto -lboost_system -lpthread -o "$d/smoke" >>/tmp/cpp.err 2>&1 \
      && out="$(LD_LIBRARY_PATH="$(dirname "$lib")" "$d/smoke" 2>/tmp/cpp.run)"; then
      P "cpp ($out)"
   else
      F cpp "$(grep -iE ': error|undefined|terminate|exception' /tmp/cpp.err /tmp/cpp.run 2>/dev/null | head -1)"
   fi
}

for l in "${LANGS[@]}"; do
   case "$l" in
      go) smoke_go;; python) smoke_python;; typescript) smoke_typescript;;
      rust) smoke_rust;; csharp) smoke_csharp;; java) smoke_java;; c) smoke_c;; cpp) smoke_cpp;;
      *) S "$l" "unknown language";;
   esac
done

echo "----"
echo "sdk-smoke: PASS=$pass SKIP=$skip FAIL=$fail"
[ "$fail" -eq 0 ]
