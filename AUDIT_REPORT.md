# HTTrackClone Repository Audit

## A) Entrypoints and build/test execution
- [Confirmed] CLI entrypoint: `src/httrack.c` (`main`).
- [Confirmed] Web/server entrypoint: `src/htsweb.c` (`main`).
- [Confirmed] Build system: Autotools (`configure.ac`, `Makefile.am`, `src/Makefile.am`).
- [Confirmed] Tests: `tests/*.test` with `tests/run-all-tests.sh`.

## B) First 10 files inspected (and why)
1. [Confirmed] `README.md` (scope/build intent)
2. [Confirmed] `configure.ac` (hardening flags/deps)
3. [Confirmed] `src/Makefile.am` (binaries + linked/vendored code)
4. [Confirmed] `src/httrack.c` (CLI initialization)
5. [Confirmed] `src/htsweb.c` (server setup + SID generation)
6. [Confirmed] `src/htsserver.c` (HTTP parse + command dispatch)
7. [Confirmed] `src/htscore.c` (post-file command execution)
8. [Confirmed] `src/htscoremain.c` (dangerous option parsing)
9. [Confirmed] `src/webhttrack` (launcher shell script)
10. [Confirmed] `tests/Makefile.am` + `tests/run-all-tests.sh` (test harness)

## C) Missing context
- [Unknown] Explicit target OS matrix for this fork.
- [Confirmed] Intended use appears desktop/offline mirroring.
- [Confirmed] Risk tolerance: medium.

## 1) EXECUTIVE SUMMARY
- [Confirmed] Repo is a C-based offline website copier with CLI + local web control server.
- [Confirmed] Biggest security risk: SID validation bypass in `htsserver`.
- [Confirmed] Biggest exposure risk: server bind defaults to wildcard interface.
- [Confirmed] Biggest correctness/release risk: required submodule `src/coucal` uninitialized in this checkout; build fails.
- [Confirmed] Explicit `system()` execution path exists (`-V` / post-processing command).
- [Inference] For medium-risk desktop usage, control-plane hardening and build reproducibility are release priorities.

### Top 5 priorities
1. [Confirmed] Fix SID check ordering bug (auth bypass)
2. [Confirmed] Bind server to loopback by default
3. [Confirmed] Replace predictable SID RNG with CSPRNG
4. [Confirmed] Enforce submodule bootstrap in docs/CI
5. [Confirmed] Constrain shell execution paths / reduce `system()` usage

## 2) REPO MAP
### Architecture/data flow
- [Confirmed] `httrack` sets callbacks and runs crawler engine.
- [Confirmed] `htsserver` parses HTTP vars and triggers crawl commands.
- [Confirmed] `webhttrack` launches `htsserver`, discovers URL, opens browser.
- [Inference] Control flow: browser params -> `smallserver` parse -> coucal store -> command dispatch -> crawler thread.

### Build/run pipeline
- [Confirmed] Build path: `./configure && make`.
- [Confirmed] Tests are shell/Automake based.
- [Confirmed] Runtime SID stored in process memory via key/value store.
- [Unknown] No signed-release/provenance workflow visible in repo.

### Dependency map
- [Confirmed] External libs: OpenSSL, zlib, pthread, dl, sockets.
- [Confirmed] Bundled code: `src/minizip/*`; submodule: `src/coucal`.
- [Inference] Vendored dependencies without automated update checks increase stale-CVE risk.

## 3) FINDINGS (DETAILED)

### Security

#### AUD-001
- Severity + Confidence: **High / High**
- Evidence:
  - [Confirmed] `src/htsserver.c` overwrites `sid` from request `_sid` before comparison.
  - [Confirmed] It then compares `sid` vs `_sid`, which can trivially match.
- Explanation:
  - [Confirmed] This defeats intended session validation.
- Risk:
  - Impact [Confirmed]: Unauthorized command/control of crawler endpoints.
  - Likelihood [Inference]: Medium-high if server reachable outside local trust boundary.
- Fix:
  - [Confirmed] Keep immutable server-side SID; compare request SID against it; never copy request into canonical SID.
  - Suggested patch:
```diff
--- a/src/htsserver.c
+++ b/src/htsserver.c
@@
-/* SID check */
-if (coucal_readptr(NewLangList, "_sid", &adr)) {
-  coucal_write(NewLangList, "sid", (intptr_t) strdup((char *) adr));
-}
+/* SID check: request _sid must equal server sid */
+if (coucal_readptr(NewLangList, "sid", &adr) &&
+    coucal_readptr(NewLangList, "_sid", &adr2)) {
+  if (strcmp((char*)adr, (char*)adr2) != 0) {
+    meth = 0;
+  }
+}
```
- Test/Verification:
  - [Confirmed] Invalid `_sid` requests must be rejected and not trigger command actions.

#### AUD-002
- Severity + Confidence: **High / High**
- Evidence:
  - [Confirmed] `src/htsserver.c` initializes bind address with `SOCaddr_initany(server)` then binds/listens.
- Explanation:
  - [Confirmed] Wildcard bind may expose control interface beyond localhost.
- Risk:
  - Impact [Confirmed]: Remote trigger surface for crawler control.
  - Likelihood [Inference]: Medium depending on network/firewall defaults.
- Fix:
  - [Confirmed] Default loopback bind; explicit opt-in for non-local listen.
  - Suggested patch:
```diff
--- a/src/htsserver.c
+++ b/src/htsserver.c
@@
-SOCaddr_initany(server);
+SOCaddr_initloopback(server);
```
- Test/Verification:
  - [Confirmed] `ss -ltnp` shows loopback listener only by default.

#### AUD-003
- Severity + Confidence: **Medium / High**
- Evidence:
  - [Confirmed] `src/htsweb.c` uses `srand(time)` + `rand()` + MD5 for SID.
- Explanation:
  - [Confirmed] Predictable seed/source; MD5 doesn’t create entropy.
- Risk:
  - Impact [Confirmed]: SID guessing feasibility increases.
  - Likelihood [Inference]: Medium in exposed scenarios.
- Fix:
  - [Confirmed] Replace with OS CSPRNG bytes.
- Test/Verification:
  - [Inference] Add SID generation tests for uniqueness/format.

#### AUD-004
- Severity + Confidence: **Medium / High**
- Evidence:
  - [Confirmed] `src/htscore.c` executes `system(temp)`.
  - [Confirmed] `src/htscoremain.c` exposes `-V` system-command option; help marks it dangerous.
- Explanation:
  - [Confirmed] Shell execution is high-risk if input path becomes untrusted.
- Risk:
  - Impact [Confirmed]: Local code execution as running user.
  - Likelihood [Inference]: Medium.
- Fix:
  - [Confirmed] Prefer argv-based `exec*` path or gate feature harder in web mode.
- Test/Verification:
  - [Confirmed] Validate expected command behavior and metacharacter handling.

### Correctness / Bugs

#### AUD-005
- Severity + Confidence: **High / High**
- Evidence:
  - [Confirmed] `.gitmodules` requires `src/coucal`.
  - [Confirmed] `git submodule status` shows uninitialized marker (`-`).
  - [Confirmed] `make -j4` fails: missing `coucal/coucal.c` rule.
- Explanation:
  - [Confirmed] Fresh checkout in current state is non-buildable without submodule init.
- Risk:
  - Impact [Confirmed]: Build/release blocked.
  - Likelihood [Confirmed]: High (reproduced).
- Fix:
  - [Confirmed] Add bootstrap guard/docs/CI step for recursive submodule init.
- Test/Verification:
  - [Confirmed] Build succeeds after submodule init.

#### AUD-006
- Severity + Confidence: **Medium / Medium**
- Evidence:
  - [Confirmed] `src/htsweb.c::back_launch_cmd` performs manual quote-toggle split without escape handling.
- Explanation:
  - [Inference] Complex quoted/escaped payloads can be misparsed.
- Risk:
  - Impact [Inference]: wrong options/job failures.
  - Likelihood [Inference]: Medium.
- Fix:
  - [Confirmed] Replace with robust parser or structured argument passing.
- Test/Verification:
  - [Inference] Add regression tests for escaped quotes/backslashes.

### Reliability / DevEx / Compliance

#### AUD-007
- Severity + Confidence: **Medium / High**
- Evidence:
  - [Confirmed] Tests exist in shell scripts; no repo CI workflows detected.
- Explanation:
  - [Inference] Regressions may pass unobserved.
- Risk:
  - Impact [Inference]: stability drift.
  - Likelihood [Inference]: Medium.
- Fix:
  - [Confirmed] Add minimal CI for configure/build + offline tests.
- Test/Verification:
  - [Confirmed] CI required status checks on PR.

#### AUD-008
- Severity + Confidence: **Low / High**
- Evidence:
  - [Confirmed] No `SECURITY.md`, `CONTRIBUTING.md`, `CODEOWNERS` found.
- Explanation:
  - [Inference] Slower vuln triage and inconsistent contribution process.
- Risk:
  - Impact [Inference]: process overhead.
  - Likelihood [Inference]: Medium.
- Fix:
  - [Confirmed] Add these standard governance docs.
- Test/Verification:
  - [Confirmed] Files present and linked in README.

#### AUD-009
- Severity + Confidence: **Low / Medium**
- Evidence:
  - [Confirmed] `COPYING`/`license.txt` exist; vendored third-party code present.
- Explanation:
  - [Inference] No consolidated third-party notices/SBOM visible.
- Risk:
  - Impact [Inference]: packaging/compliance friction.
  - Likelihood [Inference]: Medium.
- Fix:
  - [Confirmed] Add `THIRD_PARTY_NOTICES.md` + SBOM generation.
- Test/Verification:
  - [Inference] Validate notice coverage against vendored deps.

#### AUD-010
- Severity + Confidence: **Low / Low**
- Evidence:
  - [Unknown] No runtime perf measurements executed.
- Explanation:
  - [Confirmed] No defensible hot-path claims without profiling.
- Risk:
  - Impact [Unknown]
  - Likelihood [Unknown]
- Fix:
  - [Confirmed] Add benchmark fixture and baseline profile collection.
- Test/Verification:
  - [Confirmed] Compare perf metrics pre/post change.

## 4) DEPENDENCIES & SUPPLY CHAIN CHECK
- [Confirmed] No package-manager lockfiles; dependencies resolved at build/link time.
- [Confirmed] Required submodule `src/coucal` currently uninitialized in checkout.
- [Confirmed] Vendored minizip code present.
- [Unknown] CVE state of vendored code cannot be proven from this snapshot alone.

Recommendations:
- [Confirmed] Enforce submodule init in bootstrap + CI.
- [Confirmed] Add SBOM generation and release checksums.
- [Inference] Add periodic dependency review automation (Dependabot/Renovate equivalent where applicable).

## 5) ACTION PLAN (PRIORITIZED)

### Phase 0 (1–2h)
1. [Confirmed] Fix SID check (owner: security/C maintainer, 1h, low risk, accept: invalid SID blocked)
2. [Confirmed] Loopback-only bind default (owner: C maintainer, 1h, medium risk due UX, accept: localhost-only listener)
3. [Confirmed] Submodule bootstrap guard/docs (owner: maintainer, 30m, low risk, accept: fast-fail when missing)

### Phase 1 (1–2d)
1. [Confirmed] CSPRNG SID generation (owner: security, 0.5d)
2. [Confirmed] Minimal CI (configure/build/offline tests) (owner: DevEx, 1d)
3. [Confirmed] Add SECURITY/CONTRIBUTING/CODEOWNERS (owner: maintainer, 0.5d)

### Phase 2 (1–2w)
1. [Confirmed] Replace shell-based command execution where feasible (owner: core maintainer, 3–5d)
2. [Confirmed] Add notices + SBOM/provenance basics (owner: release eng, 2–3d)
3. [Confirmed] Parser hardening + targeted fuzzing (owner: security/QA, 3–5d)

Acceptance criteria per phase:
- [Confirmed] Phase 0 complete when auth bypass + remote exposure defaults + bootstrap breakage are resolved and tested.
- [Confirmed] Phase 1 complete when CI is gating and governance docs are live.
- [Confirmed] Phase 2 complete when execution/parser hardening and supply-chain metadata ship in release flow.

## 6) GOOD CITIZEN IMPROVEMENTS
- [Confirmed] README: add explicit recursive submodule bootstrap and tested commands.
- [Confirmed] Add `CONTRIBUTING.md`, `SECURITY.md`, `CODEOWNERS`, issue templates.
- [Confirmed] Minimal CI: build + offline tests on PR; scheduled online crawl tests.
- [Confirmed] Add style/lint guidance and commit convention.

## Commands executed (evidence)
- [Confirmed] `./configure --prefix=/tmp/httrack-audit-install` (success)
- [Confirmed] `make -j4` (fails: missing `coucal/coucal.c` target)
- [Confirmed] `git submodule status` (shows uninitialized `src/coucal`)
- [Confirmed] multiple `rg`/`sed` inspections across `src/*`, `tests/*`, `configure.ac`, `README.md`
