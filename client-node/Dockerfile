FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install --no-install-recommends -y \
    bash \
    ca-certificates \
    curl \
    ethtool \
    iproute2 \
    iputils-ping \
    net-tools \
    netcat-openbsd \
    python3 \
    python3-pip \
    python3-venv \
    tcpdump \
    && rm -rf /var/lib/apt/lists/*

# Optional support for Python projects that use uv.
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /usr/local/bin/

WORKDIR /app

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENV PYTHONUNBUFFERED=1

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["/bin/sh"]
