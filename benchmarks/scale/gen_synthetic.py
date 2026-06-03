#!/usr/bin/env python3
"""Generate a synthetic 1M-scale memory benchmark dataset.

Usage:
  python3 benchmarks/scale/gen_synthetic.py \\
    --num-memories 10000 --num-questions 100 \\
    --output data/scale/synth_1m.json

Output format:
  {
    "memories": [{"id": "m0", "content": "...", "tags": []}, ...],
    "questions": [{"id": "q0", "question": "...", "gold_memory_id": "m0", "gold_answer": "..."}, ...],
    "metadata": {"num_memories": N, "approx_tokens": M, "seed": 42}
  }
"""

from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path

_TOPICS = [
    "artificial intelligence", "machine learning", "deep learning", "neural networks",
    "natural language processing", "computer vision", "robotics", "quantum computing",
    "cryptography", "distributed systems", "databases", "operating systems",
    "compilers", "algorithms", "data structures", "software engineering",
    "climate change", "renewable energy", "ocean currents", "atmospheric science",
    "genomics", "protein folding", "neuroscience", "epidemiology",
    "history of science", "philosophy of mind", "cognitive psychology", "linguistics",
    "number theory", "topology", "abstract algebra", "differential equations",
    "economics", "game theory", "behavioral finance", "supply chain management",
]

_TEMPLATES = [
    "{topic} is a field that explores {aspect}. Key researchers include {name1} and {name2}. "
    "The main challenge is {challenge}. Recent progress has been made in {subfield}.",

    "In {year}, {name1} proposed a new approach to {topic} that {action}. "
    "This was significant because {reason}. The method relies on {technique}.",

    "The {topic} team at {org} demonstrated that {claim}. "
    "Their paper, titled '{paper_title}', achieved {metric} on the {benchmark} benchmark. "
    "The key insight was {insight}.",

    "{name1}'s work on {topic} showed that {observation}. "
    "This contradicted the earlier assumption that {old_assumption}. "
    "The finding was later replicated by {name2} in {year}.",

    "A comparison of {method1} and {method2} for {topic} revealed that {result}. "
    "Under conditions where {condition}, {method1} outperforms {method2} by {margin}. "
    "Practitioners should consider {recommendation}.",
]

_NAMES = [
    "Alice Chen", "Bob Martinez", "Carol Patel", "David Kim", "Eva Mueller",
    "Frank Nguyen", "Grace Liu", "Henry Wang", "Isabel Rossi", "James Okafor",
    "Karen Schmidt", "Liam O'Brien", "Maria Santos", "Nadia Volkova", "Omar Hassan",
]

_ORGS = [
    "MIT", "Stanford", "Berkeley", "CMU", "Oxford", "ETH Zurich",
    "DeepMind", "OpenAI", "Google Brain", "Meta AI", "Microsoft Research",
]

_ASPECTS = [
    "the relationship between structure and function",
    "emergent behaviors in complex systems",
    "optimization under uncertainty",
    "representation and generalization",
    "scalability and efficiency",
    "robustness and reliability",
    "interpretability and transparency",
]

_CHALLENGES = [
    "handling distribution shift",
    "achieving sample efficiency",
    "scaling to real-world complexity",
    "maintaining coherence over long sequences",
    "bridging theory and practice",
]

_ACTIONS = [
    "significantly reduced computational costs",
    "improved accuracy by a substantial margin",
    "enabled real-time processing",
    "generalized across domains without fine-tuning",
    "demonstrated surprising emergent capabilities",
]


def _random_memory(idx: int, rng: random.Random) -> dict[str, object]:
    topic = rng.choice(_TOPICS)
    template = rng.choice(_TEMPLATES)
    name1 = rng.choice(_NAMES)
    name2 = rng.choice([n for n in _NAMES if n != name1])
    year = rng.randint(2015, 2024)
    org = rng.choice(_ORGS)
    aspect = rng.choice(_ASPECTS)
    challenge = rng.choice(_CHALLENGES)
    action = rng.choice(_ACTIONS)

    content = template.format(
        topic=topic,
        aspect=aspect,
        name1=name1,
        name2=name2,
        year=year,
        org=org,
        challenge=challenge,
        action=action,
        claim=f"{topic} models can {action}",
        reason=f"it addressed the challenge of {challenge}",
        technique=f"a novel variant of {rng.choice(_ASPECTS)}",
        paper_title=f"Advances in {topic.title()}: A {year} Perspective",
        metric=f"{rng.randint(70, 99)}%",
        benchmark=f"{topic.split()[0].upper()}-Bench",
        insight=f"that {aspect} is key to {challenge}",
        observation=f"{aspect} correlates with {challenge}",
        old_assumption=f"{aspect} was irrelevant to {challenge}",
        method1=f"{topic.split()[0]}-Net",
        method2=f"vanilla {topic.split()[0]}",
        result=f"{topic.split()[0]}-Net excels at {aspect}",
        condition=aspect,
        margin=f"{rng.randint(5, 30)}%",
        recommendation=f"applying {action} for {challenge}",
        subfield=f"{topic} + {rng.choice(_TOPICS)}",
    )

    fact_key = f"fact_{idx:06d}_{topic.replace(' ', '_')}"
    return {"id": f"m{idx}", "key": fact_key, "content": content, "tags": [topic]}


def _random_question(idx: int, memory: dict[str, object], rng: random.Random) -> dict[str, object]:
    content = str(memory["content"])
    words = content.split()
    topic_words = [w for w in words if len(w) > 5]
    keyword = rng.choice(topic_words) if topic_words else words[0]

    question = f"What do you know about {keyword.lower().strip('.,;:')}?"
    gold_answer = content[:300] if len(content) > 300 else content

    return {
        "id": f"q{idx}",
        "question": question,
        "gold_memory_id": str(memory["id"]),
        "gold_memory_key": str(memory["key"]),
        "gold_answer": gold_answer,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--num-memories", type=int, default=10_000)
    parser.add_argument("--num-questions", type=int, default=100)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    rng = random.Random(args.seed)

    print(f"Generating {args.num_memories} memories...", file=sys.stderr)
    memories = [_random_memory(i, rng) for i in range(args.num_memories)]

    print(f"Generating {args.num_questions} questions...", file=sys.stderr)
    question_memories = rng.sample(memories, min(args.num_questions, len(memories)))
    questions = [_random_question(i, m, rng) for i, m in enumerate(question_memories)]

    approx_tokens = sum(len(str(m["content"])) // 4 for m in memories)

    dataset = {
        "memories": memories,
        "questions": questions,
        "metadata": {
            "num_memories": len(memories),
            "num_questions": len(questions),
            "approx_tokens": approx_tokens,
            "seed": args.seed,
        },
    }

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(dataset, indent=2))
    print(
        f"Generated {len(memories)} memories (~{approx_tokens:,} tokens), "
        f"{len(questions)} questions → {args.output}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
