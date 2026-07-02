import { useCallback, useEffect, useRef, useState } from "react";
import { Panel, Badge, Spinner } from "@rakuensoftware/smoothgui";
import type { BadgeVariant } from "@rakuensoftware/smoothgui";
import { renderMd } from "./chat/markdown";

/* ---- API types (mirror the enriched /api/workflow/items* envelopes) ---- */

interface Item {
  id: string;
  workflow: string;
  version: string;
  stage: string;
  state: string; // active | accepted | rejected | abandoned
  mode: string;
  pause_reason: string;
  repo: string;
  proposal_name?: string;
  pr_ref?: string;
  submitter?: string;
  cum_cost_usd?: number;
  work_item_max_cost_usd?: number;
  override_count?: number;
}
interface WfEvent {
  id: number;
  stage: string;
  kind: string;
  actor: string;
  detail: string;
  cost_usd: number;
  created_at: string;
}

const POLL_MS = 4000;

async function getJSON<T>(url: string): Promise<T> {
  const r = await fetch(url, { headers: { "X-CSRF-Token": window._csrf || "" } });
  if (!r.ok) throw new Error(`HTTP ${r.status}`);
  return (await r.json()) as T;
}
async function postJSON<T>(url: string, body: unknown): Promise<{ status: number; data: T }> {
  const r = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json", "X-CSRF-Token": window._csrf || "" },
    body: JSON.stringify(body),
  });
  let data = {} as T;
  try {
    data = (await r.json()) as T;
  } catch {
    /* empty body ok */
  }
  return { status: r.status, data };
}

const isTerminal = (s: string) => s === "accepted" || s === "rejected" || s === "abandoned";

// A human status label + tone from the row's state + pause_reason + stage. Derived
// strictly from the documented enums — no invented "drafting" state.
function statusOf(it: Item): { label: string; variant: BadgeVariant } {
  if (it.state === "accepted") return { label: "merged (accepted)", variant: "success" };
  if (it.state === "rejected") return { label: "rejected", variant: "error" };
  if (it.state === "abandoned") return { label: "abandoned", variant: "neutral" };
  // active:
  switch (it.pause_reason) {
    case "pending_human":
      return { label: `awaiting approval · ${it.stage}`, variant: "warning" };
    case "ci_pending":
      return { label: "CI running", variant: "running" };
    case "merge_pending":
      return { label: "merging", variant: "running" };
    case "panel_degraded":
    case "panel_unreachable":
      return { label: `parked · ${it.pause_reason}`, variant: "warning" };
    case "budget_exceeded":
      return { label: "parked · budget exceeded", variant: "error" };
    case "failed":
      return { label: "parked · failed", variant: "error" };
    case "max_attempts":
      return { label: "parked · max attempts", variant: "error" };
    default:
      return { label: `running · ${it.stage}`, variant: "running" };
  }
}

// A lifecycle event kind → a compact human verb for the timeline.
const KIND_LABEL: Record<string, string> = {
  create: "created",
  advance: "advanced",
  loop: "looped back",
  pause: "paused",
  failed: "failed",
  terminal: "completed",
  resume: "resumed",
  approve: "approved",
  reject: "rejected",
  reject_retry: "rejected (retry)",
  override: "overridden",
  rejected: "rejected",
};

// pr_ref is opaque (a PR number or a URL). Render a link when it looks like one.
function prHref(pr: string): string | null {
  if (/^https?:\/\//.test(pr)) return pr;
  return null;
}

export default function Proposals() {
  const [items, setItems] = useState<Item[]>([]);
  const [showAll, setShowAll] = useState(false);
  const [selId, setSelId] = useState<string | null>(null);
  const [detail, setDetail] = useState<Item | null>(null);
  const [events, setEvents] = useState<WfEvent[]>([]);
  const [proposalMd, setProposalMd] = useState<string>("");
  const [proposalTrunc, setProposalTrunc] = useState(false);
  const [loading, setLoading] = useState(false);
  const [status, setStatus] = useState<{ kind: "ok" | "err"; msg: string } | null>(null);
  const [gateMsg, setGateMsg] = useState("");
  const afterRef = useRef(0); // events pagination cursor for the open proposal
  const reqRef = useRef(0); // monotonic open token: stale async loads (older selection) no-op

  const refreshList = useCallback(() => {
    getJSON<{ items: Item[] }>(showAll ? "/api/workflow/items/all" : "/api/workflow/items")
      .then((d) => setItems(d.items || []))
      .catch((e) =>
        setStatus({ kind: "err", msg: showAll ? `list all failed: ${e.message}` : "list failed" }),
      );
  }, [showAll]);

  useEffect(() => {
    refreshList();
  }, [refreshList]);

  // Load a proposal's full detail: the item row, its source markdown, and the head
  // of its event timeline (resetting the pagination cursor).
  const openProposal = useCallback((id: string) => {
    const myReq = ++reqRef.current; // invalidates any still-in-flight prior load
    const live = () => myReq === reqRef.current;
    setSelId(id);
    setLoading(true);
    setEvents([]);
    afterRef.current = 0;
    setGateMsg("");
    Promise.all([
      getJSON<Item>(`/api/workflow/items/${encodeURIComponent(id)}`)
        .then((d) => live() && setDetail(d))
        .catch(() => live() && setDetail(null)),
      getJSON<{ proposal_md: string; truncated: boolean }>(
        `/api/workflow/items/${encodeURIComponent(id)}/proposal`,
      )
        .then((d) => {
          if (!live()) return;
          setProposalMd(d.proposal_md || "");
          setProposalTrunc(!!d.truncated);
        })
        .catch(() => {
          if (!live()) return;
          setProposalMd("");
          setProposalTrunc(false);
        }),
      getJSON<{ events: WfEvent[]; next_after: number }>(
        `/api/workflow/items/${encodeURIComponent(id)}/events?limit=200`,
      )
        .then((d) => {
          if (!live()) return;
          setEvents(d.events || []);
          afterRef.current = d.next_after || 0;
        })
        .catch(() => {}),
    ]).finally(() => {
      if (live()) setLoading(false);
    });
  }, []);

  // Poll the open proposal while it's active: refresh the row and append only new
  // events (after=cursor, so no re-fetch/dup). Keyed on selId alone (not detail) so
  // it isn't torn down every tick; the interval self-stops once the item reaches a
  // terminal state, and cleans up on unmount / selection change.
  useEffect(() => {
    if (!selId) return;
    let cancelled = false;
    const iv = window.setInterval(async () => {
      try {
        const it = await getJSON<Item>(`/api/workflow/items/${encodeURIComponent(selId)}`);
        if (cancelled) return;
        setDetail(it);
        setItems((prev) => prev.map((p) => (p.id === it.id ? it : p)));
        const d = await getJSON<{ events: WfEvent[]; next_after: number }>(
          `/api/workflow/items/${encodeURIComponent(selId)}/events?after=${afterRef.current}&limit=200`,
        );
        if (cancelled) return;
        if (d.events && d.events.length) {
          // Idempotent append: keep only events past the last one we hold, so no
          // race (re-open, overlapping tick) can duplicate a row.
          setEvents((prev) => {
            const lastId = prev.length ? prev[prev.length - 1].id : 0;
            const fresh = d.events.filter((e) => e.id > lastId);
            return fresh.length ? [...prev, ...fresh] : prev;
          });
          afterRef.current = d.next_after || afterRef.current;
        }
        if (isTerminal(it.state)) window.clearInterval(iv);
      } catch {
        /* transient; next tick retries */
      }
    }, POLL_MS);
    return () => {
      cancelled = true;
      window.clearInterval(iv);
    };
  }, [selId]);

  const decideGate = useCallback(
    async (decision: "approve" | "reject") => {
      if (!selId) return;
      setGateMsg("");
      const { status: st, data } = await postJSON<{ error?: string }>(
        `/api/workflow/items/${encodeURIComponent(selId)}/gate`,
        { decision },
      );
      if (st >= 200 && st < 300) {
        setGateMsg(decision === "approve" ? "Approved — resuming." : "Rejected.");
        openProposal(selId); // refresh detail + events after the decision
      } else {
        setGateMsg(data.error || `failed (HTTP ${st})`);
      }
    },
    [selId, openProposal],
  );

  const canDecide = !!detail && detail.pause_reason === "pending_human";

  return (
    <div style={{ display: "flex", height: "100%", fontFamily: "system-ui" }}>
      {/* left rail: proposal list */}
      <div
        style={{
          width: 300,
          flexShrink: 0,
          borderRight: "1px solid #eee",
          overflowY: "auto",
          padding: 12,
        }}
      >
        <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 8 }}>
          <strong style={{ fontSize: 16 }}>Proposals</strong>
          <Badge label={`${items.length}`} variant="neutral" />
          <button onClick={refreshList} style={btn}>
            Refresh
          </button>
        </div>
        <label style={{ fontSize: 12, color: "#666", display: "flex", alignItems: "center", gap: 6 }}>
          <input type="checkbox" checked={showAll} onChange={(e) => setShowAll(e.target.checked)} />
          Show all (operator)
        </label>
        {status && (
          <div style={{ fontSize: 12, color: status.kind === "err" ? "#c00" : "#070", marginTop: 6 }}>
            {status.msg}
          </div>
        )}
        <div style={{ marginTop: 8 }}>
          {items.length === 0 && (
            <div style={{ color: "#999", fontSize: 13 }}>No proposals yet.</div>
          )}
          {items.map((it) => {
            const s = statusOf(it);
            return (
              <div
                key={it.id}
                onClick={() => openProposal(it.id)}
                style={{
                  padding: "8px 8px",
                  borderRadius: 6,
                  cursor: "pointer",
                  marginBottom: 4,
                  background: selId === it.id ? "#eef4ff" : "transparent",
                  border: "1px solid",
                  borderColor: selId === it.id ? "#bcd4ff" : "#f0f0f0",
                }}
              >
                <div style={{ display: "flex", justifyContent: "space-between", gap: 6 }}>
                  <span style={{ fontWeight: 600, fontSize: 13, wordBreak: "break-all" }}>
                    {it.proposal_name || it.id}
                  </span>
                </div>
                <div style={{ display: "flex", alignItems: "center", gap: 6, marginTop: 4 }}>
                  <Badge label={s.label} variant={s.variant} />
                  <span style={{ fontSize: 11, color: "#999" }}>{it.workflow}</span>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      {/* main: selected proposal detail */}
      <div style={{ flex: 1, minWidth: 0, overflowY: "auto", padding: 16 }}>
        {!detail && (
          <div style={{ color: "#888", fontSize: 14, marginTop: 20 }}>
            Select a proposal to see its status and history.
            <Spinner loading={loading} text="loading…" />
          </div>
        )}
        {detail && (
          <>
            <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 4 }}>
              <strong style={{ fontSize: 18 }}>{detail.proposal_name || detail.id}</strong>
              <Badge label={statusOf(detail).label} variant={statusOf(detail).variant} />
              <Spinner loading={loading} text="" />
            </div>
            <StatusHeader item={detail} />

            {canDecide && (
              <Panel title={`Human gate · ${detail.stage}`}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <button onClick={() => void decideGate("approve")} style={btn}>
                    Approve
                  </button>
                  <button
                    onClick={() => void decideGate("reject")}
                    style={{ ...btn, background: "#fbeaea", color: "#a33", borderColor: "#e0a0a0" }}
                  >
                    Reject
                  </button>
                  {gateMsg && <span style={{ fontSize: 12, color: "#667" }}>{gateMsg}</span>}
                </div>
              </Panel>
            )}

            <Panel title={`History · ${events.length} events`}>
              <Timeline events={events} />
            </Panel>

            <Panel title="Proposal">
              {proposalTrunc && (
                <div style={{ fontSize: 12, color: "#a60", marginBottom: 6 }}>
                  ⚠ Proposal truncated (over the size cap); showing the first part.
                </div>
              )}
              {proposalMd ? (
                <div
                  style={{ fontSize: 13, lineHeight: 1.5, overflowWrap: "anywhere" }}
                  dangerouslySetInnerHTML={{ __html: renderMd(proposalMd) }}
                />
              ) : (
                <div style={{ color: "#999", fontSize: 13 }}>No proposal markdown.</div>
              )}
            </Panel>
          </>
        )}
      </div>
    </div>
  );
}

function StatusHeader({ item }: { item: Item }) {
  const pr = item.pr_ref ? prHref(item.pr_ref) : null;
  return (
    <div
      style={{
        display: "grid",
        gridTemplateColumns: "repeat(auto-fill, minmax(180px, 1fr))",
        gap: "2px 16px",
        fontSize: 13,
        margin: "6px 0 10px",
      }}
    >
      <Field k="stage" v={item.stage} />
      <Field k="workflow" v={`${item.workflow} · v${(item.version || "").slice(0, 8)}`} />
      {item.repo ? <Field k="repo" v={item.repo} /> : null}
      {typeof item.cum_cost_usd === "number" && (
        <Field
          k="cost"
          v={
            item.work_item_max_cost_usd
              ? `$${item.cum_cost_usd.toFixed(4)} / $${item.work_item_max_cost_usd.toFixed(2)}`
              : `$${item.cum_cost_usd.toFixed(4)}`
          }
        />
      )}
      {item.pr_ref ? (
        <div style={{ display: "flex", gap: 8 }}>
          <span style={{ color: "#888" }}>PR</span>
          {pr ? (
            <a href={pr} target="_blank" rel="noreferrer" style={{ color: "#2563eb" }}>
              {item.pr_ref}
            </a>
          ) : (
            <span style={{ fontFamily: "monospace" }}>{item.pr_ref}</span>
          )}
        </div>
      ) : null}
      {item.override_count ? <Field k="overrides" v={String(item.override_count)} /> : null}
      {item.submitter ? <Field k="submitter" v={item.submitter} /> : null}
    </div>
  );
}

function Timeline({ events }: { events: WfEvent[] }) {
  if (!events.length) return <div style={{ color: "#999", fontSize: 13 }}>No events yet.</div>;
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 0 }}>
      {events.map((e) => (
        <div
          key={e.id}
          style={{
            display: "flex",
            gap: 10,
            padding: "6px 0",
            borderBottom: "1px solid #f3f3f3",
            fontSize: 13,
          }}
        >
          <span style={{ color: "#aaa", fontSize: 11, width: 130, flexShrink: 0 }}>
            {e.created_at}
          </span>
          <span style={{ flexShrink: 0, width: 120 }}>
            <Badge label={KIND_LABEL[e.kind] || e.kind} variant="neutral" />
          </span>
          <span style={{ flex: 1, minWidth: 0 }}>
            <span style={{ color: "#555" }}>{e.stage}</span>
            {e.detail ? <span style={{ color: "#888" }}> — {e.detail}</span> : null}
            <span style={{ color: "#bbb", fontSize: 11 }}>
              {" "}
              ({e.actor}
              {e.cost_usd ? `, $${e.cost_usd.toFixed(4)}` : ""})
            </span>
          </span>
        </div>
      ))}
    </div>
  );
}

function Field({ k, v }: { k: string; v: string }) {
  return (
    <div style={{ display: "flex", gap: 8 }}>
      <span style={{ color: "#888" }}>{k}</span>
      <span style={{ overflowWrap: "anywhere" }}>{v}</span>
    </div>
  );
}

const btn: React.CSSProperties = {
  fontSize: 13,
  padding: "4px 10px",
  border: "1px solid #ccc",
  borderRadius: 4,
  background: "#fff",
  cursor: "pointer",
};
