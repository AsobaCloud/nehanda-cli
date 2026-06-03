package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"strconv"
)

type workflowSessionInfo struct {
	ID            int    `json:"id"`
	Template      string `json:"template"`
	Channel       string `json:"channel"`
	Status        string `json:"status"`
	CurrentPhase  int    `json:"current_phase"`
	CurrentTurn   int    `json:"current_turn"`
	PhaseCount    int    `json:"phase_count"`
	TurnsInPhase  int    `json:"turns_in_phase"`
	PhaseName     string `json:"phase_name"`
	ExpectedAgent string `json:"expected_agent"`
	ExpectedRole  string `json:"expected_role"`
	PausedReason  string `json:"paused_reason"`
	CreatedAt     string `json:"created_at"`
	UpdatedAt     string `json:"updated_at"`
	NextPrompt    string `json:"next_prompt,omitempty"`
	RecentContext string `json:"recent_context,omitempty"`
}

func (s *server) mcpStructured(r *http.Request, tool string, arguments map[string]any) (json.RawMessage, error) {
	if arguments == nil {
		arguments = map[string]any{}
	}
	resp, err := s.socketCallForRequest(r, map[string]any{
		"method":    "mcp.call",
		"tool":      tool,
		"arguments": arguments,
	})
	if err != nil {
		return nil, err
	}
	raw, ok := resp["structuredContent"]
	if !ok {
		return nil, fmt.Errorf("server response missing structuredContent")
	}
	return raw, nil
}

func writeJSONError(w http.ResponseWriter, status int, msg string) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(map[string]string{"error": msg})
}

func (s *server) socketCallForRequest(r *http.Request, req map[string]any) (map[string]json.RawMessage, error) {
	ctx, cancel := context.WithTimeout(r.Context(), socketCallTimeout)
	defer cancel()
	return s.rpcV1Call(ctx, req)
}

// rpcV1Call dispatches a unary RPC over aimee-server's /v1 HTTP transport
// (POST /v1/rpc over the UDS), the cutover from the legacy NDJSON socket. The
// bridge echoes the dispatch response byte-for-byte (the server's NDJSON↔/v1
// parity gate proves this), so the decoded result is identical to the old
// socket path; the UDS is filesystem-trusted, so no bearer is needed. If the
// /v1 HTTP listener is unreachable (transport error), it falls back to the
// NDJSON socket so webchat keeps working on socket-only servers — WP3.4 drops
// the fallback once /v1 is always-on.
func (s *server) rpcV1Call(ctx context.Context, req map[string]any) (map[string]json.RawMessage, error) {
	body, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}
	st, data, err := s.v1Request(ctx, http.MethodPost, "/v1/rpc", body)
	if err != nil {
		return socketCallContext(ctx, s.cfg.socketPath, req)
	}
	var msg map[string]json.RawMessage
	if len(data) > 0 {
		if uerr := json.Unmarshal(data, &msg); uerr != nil {
			return nil, fmt.Errorf("decode /v1/rpc response: %w", uerr)
		}
	}
	if st != http.StatusOK {
		if rerr := rpcError(msg); rerr != nil {
			return nil, rerr
		}
		return nil, fmt.Errorf("/v1/rpc status %d", st)
	}
	if rerr := rpcError(msg); rerr != nil {
		return nil, rerr
	}
	return msg, nil
}

func (s *server) handleAgents(w http.ResponseWriter, r *http.Request) {
	resp, err := s.socketCallForRequest(r, map[string]any{"method": "agent.list"})
	w.Header().Set("Content-Type", "application/json")
	if err != nil {
		fmt.Fprintf(w, `{"agents":[],"count":0}`)
		return
	}
	agents, ok := resp["agents"]
	if !ok {
		fmt.Fprintf(w, `{"agents":[],"count":0}`)
		return
	}
	var arr []json.RawMessage
	json.Unmarshal(agents, &arr)
	count := len(arr)
	json.NewEncoder(w).Encode(map[string]any{"agents": arr, "count": count})
}

func (s *server) handleDelegations(w http.ResponseWriter, r *http.Request) {
	resp, err := s.socketCallForRequest(r, map[string]any{"method": "dashboard.delegations"})
	w.Header().Set("Content-Type", "application/json")
	if err != nil || resp == nil {
		fmt.Fprintf(w, "[]")
		return
	}
	if data, ok := resp["data"]; ok {
		w.Write(data)
	} else {
		fmt.Fprintf(w, "[]")
	}
}

func (s *server) handleMetrics(w http.ResponseWriter, r *http.Request) {
	resp, err := s.socketCallForRequest(r, map[string]any{"method": "dashboard.metrics"})
	w.Header().Set("Content-Type", "application/json")
	if err != nil || resp == nil {
		fmt.Fprintf(w, "[]")
		return
	}
	if data, ok := resp["data"]; ok {
		w.Write(data)
	} else {
		fmt.Fprintf(w, "[]")
	}
}

func (s *server) handleCollabRulesList(w http.ResponseWriter, r *http.Request) {
	resp, err := s.socketCallForRequest(r, map[string]any{"method": "collab_rules.list"})
	w.Header().Set("Content-Type", "application/json")
	if err != nil || resp == nil {
		fmt.Fprintf(w, "[]")
		return
	}
	if rules, ok := resp["rules"]; ok {
		w.Write(rules)
	} else {
		fmt.Fprintf(w, "[]")
	}
}

func (s *server) handleCollabRulesActive(w http.ResponseWriter, r *http.Request) {
	resp, err := s.socketCallForRequest(r, map[string]any{"method": "collab_rules.list_active"})
	w.Header().Set("Content-Type", "application/json")
	if err != nil || resp == nil {
		fmt.Fprintf(w, "{}")
		return
	}
	if active, ok := resp["active"]; ok {
		w.Write(active)
	} else {
		fmt.Fprintf(w, "{}")
	}
}

func (s *server) handleCollabRuleAction(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}
	idStr := r.PathValue("id")
	action := r.PathValue("action")
	id, err := strconv.Atoi(idStr)
	if err != nil || id <= 0 {
		http.Error(w, `{"error":"invalid id"}`, http.StatusBadRequest)
		return
	}
	switch action {
	case "approve", "reject", "retire":
	default:
		http.Error(w, `{"error":"unknown action"}`, http.StatusBadRequest)
		return
	}
	_, serr := s.socketCallForRequest(r, map[string]any{
		"method": "collab_rules." + action,
		"id":     id,
	})
	w.Header().Set("Content-Type", "application/json")
	if serr != nil {
		w.WriteHeader(http.StatusInternalServerError)
		fmt.Fprintf(w, `{"error":%q}`, serr.Error())
		return
	}
	fmt.Fprintf(w, `{"status":"ok"}`)
}

func (s *server) dataArrayHandler(method string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		resp, err := s.socketCallForRequest(r, map[string]any{"method": method})
		w.Header().Set("Content-Type", "application/json")
		if err != nil || resp == nil {
			fmt.Fprintf(w, "[]")
			return
		}
		if data, ok := resp["data"]; ok {
			w.Write(data)
		} else {
			fmt.Fprintf(w, "[]")
		}
	}
}

func (s *server) dataObjectHandler(method string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		resp, err := s.socketCallForRequest(r, map[string]any{"method": method})
		w.Header().Set("Content-Type", "application/json")
		if err != nil || resp == nil {
			fmt.Fprintf(w, "{}")
			return
		}
		if data, ok := resp["data"]; ok {
			w.Write(data)
		} else {
			fmt.Fprintf(w, "{}")
		}
	}
}

func (s *server) handleDashboardAll(w http.ResponseWriter, r *http.Request) {
	resp, err := s.socketCallForRequest(r, map[string]any{"method": "dashboard.all"})
	w.Header().Set("Content-Type", "application/json")
	if err != nil || resp == nil {
		fmt.Fprintf(w, "{}")
		return
	}
	out := map[string]json.RawMessage{}
	for k, v := range resp {
		if k == "status" {
			continue
		}
		out[k] = v
	}
	json.NewEncoder(w).Encode(out)
}

func (s *server) handleWorkflowByChannel(w http.ResponseWriter, r *http.Request) {
	channel := r.PathValue("channel")
	if channel == "" {
		writeJSONError(w, http.StatusBadRequest, "channel required")
		return
	}

	raw, err := s.mcpStructured(r, "session_list", map[string]any{"limit": 100})
	if err != nil {
		writeJSONError(w, http.StatusServiceUnavailable, err.Error())
		return
	}

	var payload struct {
		Sessions []workflowSessionInfo `json:"sessions"`
	}
	if err := json.Unmarshal(raw, &payload); err != nil {
		writeJSONError(w, http.StatusInternalServerError, "invalid workflow response")
		return
	}

	var selected *workflowSessionInfo
	for i := range payload.Sessions {
		session := &payload.Sessions[i]
		if session.Channel != channel {
			continue
		}
		if session.Status != "active" && session.Status != "paused" {
			continue
		}
		if selected == nil ||
			(session.Status == "active" && selected.Status != "active") ||
			(session.Status == selected.Status && session.UpdatedAt > selected.UpdatedAt) {
			selected = session
		}
	}

	if selected == nil {
		writeJSONError(w, http.StatusNotFound, fmt.Sprintf("workflow session for channel %q not found", channel))
		return
	}

	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(selected)
}

func (s *server) handleWorkflowPause(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil || id <= 0 {
		writeJSONError(w, http.StatusBadRequest, "invalid workflow id")
		return
	}
	var body struct {
		Reason string `json:"reason"`
	}
	_ = json.NewDecoder(r.Body).Decode(&body)
	if body.Reason == "" {
		body.Reason = "manual"
	}

	raw, err := s.mcpStructured(r, "session_pause", map[string]any{
		"id":     id,
		"reason": body.Reason,
	})
	if err != nil {
		writeJSONError(w, http.StatusServiceUnavailable, err.Error())
		return
	}

	w.Header().Set("Content-Type", "application/json")
	w.Write(raw)
}

func (s *server) handleLSPDiagnosticsSummary(w http.ResponseWriter, r *http.Request) {
	resp, err := s.socketCallForRequest(r, map[string]any{"method": "lsp.diagnostics_summary"})
	w.Header().Set("Content-Type", "application/json")
	if err != nil || resp == nil {
		fmt.Fprintf(w, `{"errors":0,"warnings":0,"active_servers":0}`)
		return
	}
	out := map[string]int{"errors": 0, "warnings": 0, "active_servers": 0}
	for _, k := range []string{"errors", "warnings", "active_servers"} {
		if v, ok := resp[k]; ok {
			var n float64
			if json.Unmarshal(v, &n) == nil {
				out[k] = int(n)
			}
		}
	}
	json.NewEncoder(w).Encode(out)
}
