# Testing Skills

A useful skill changes behavior under pressure.

Minimal compliance fixture shape:

```json
{
  "name": "scenario-name",
  "baseline_response": "The agent response without the skill.",
  "treatment_response": "The agent response with the skill.",
  "violation_check": {"type": "contains", "value": "bad behavior marker"},
  "compliance_check": {"type": "contains", "value": "good behavior marker"}
}
```

The baseline should show the target violation. The treatment should show the
desired behavior. If both are already compliant, the skill has not proven that it
adds value.
