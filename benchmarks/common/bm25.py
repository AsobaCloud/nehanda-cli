#!/usr/bin/env python3
"""Small stdlib BM25 implementation for benchmark baselines."""

from __future__ import annotations

import math
from collections import Counter


def tokenize(text: str) -> list[str]:
    tokens = []
    current = []
    for ch in text.lower():
        if ch.isalnum():
            current.append(ch)
            continue
        if current:
            tokens.append("".join(current))
            current = []
    if current:
        tokens.append("".join(current))
    return tokens


class BM25Index:
    def __init__(self, documents: list[dict[str, str]], *, k1: float = 1.5, b: float = 0.75) -> None:
        self.documents = documents
        self.k1 = k1
        self.b = b
        self.doc_terms = [Counter(tokenize(doc["content"])) for doc in documents]
        self.doc_lengths = [sum(terms.values()) for terms in self.doc_terms]
        self.avg_doc_length = sum(self.doc_lengths) / len(self.doc_lengths) if self.doc_lengths else 0.0
        self.doc_freq: Counter[str] = Counter()
        for terms in self.doc_terms:
            self.doc_freq.update(terms.keys())

    def search(self, query: str, limit: int) -> list[dict[str, str]]:
        query_terms = tokenize(query)
        if not query_terms:
            return []
        total_docs = len(self.documents)
        scored: list[tuple[float, int]] = []
        for index, terms in enumerate(self.doc_terms):
            score = 0.0
            doc_len = self.doc_lengths[index] or 1
            for term in query_terms:
                tf = terms.get(term, 0)
                if tf <= 0:
                    continue
                df = self.doc_freq.get(term, 0)
                idf = math.log(1.0 + (total_docs - df + 0.5) / (df + 0.5))
                denom = tf + self.k1 * (1.0 - self.b + self.b * doc_len / max(self.avg_doc_length, 1.0))
                score += idf * (tf * (self.k1 + 1.0)) / denom
            if score > 0.0:
                scored.append((score, index))
        scored.sort(key=lambda item: (-item[0], item[1]))
        return [self.documents[index] for _, index in scored[:limit]]
