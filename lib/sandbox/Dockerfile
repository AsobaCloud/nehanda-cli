FROM node:22-bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends python3 git bash \
    && rm -rf /var/lib/apt/lists/*

ENV HOME=/tmp PYTHONDONTWRITEBYTECODE=1
WORKDIR /workspace
USER 65534:65534
ENTRYPOINT ["/bin/bash"]
