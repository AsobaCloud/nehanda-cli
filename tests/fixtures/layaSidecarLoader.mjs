/** #36: isolate the external Laya runtime for CLI subprocess acceptance tests. */
export async function load(url, context, nextLoad) {
  if (url.endsWith('/lib/laya_sidecar.mjs')) {
    return { format: 'module', shortCircuit: true, source: `
      export async function ensureLayaReady() {}
      export async function layaRequest() { return {ok:false,error:'deterministic CLI controller fixture'} }
      export async function loadLaya() { return {ok:false} }
      export async function pingLaya() { return {ok:false} }
      export async function shutdownLaya() {}
      export function resetLayaSidecar() {}
      export const LAYA_DEFAULT_TIMEOUT_MS = 250;
    ` }
  }
  return nextLoad(url, context)
}
