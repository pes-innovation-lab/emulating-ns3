FROM alpine:3.20

RUN apk add --no-cache python3 frr iproute2 ethtool bash

RUN mkdir -p /var/run/frr /var/log/frr && \
    chown -R frr:frr /var/run/frr /var/log/frr

COPY frr.conf /etc/frr/frr.conf
COPY daemons /etc/frr/daemons

RUN chown -R frr:frr /etc/frr && \
    chmod 640 /etc/frr/frr.conf /etc/frr/daemons

WORKDIR /app
COPY main.py /app/main.py
COPY entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh

ENTRYPOINT ["/bin/bash", "/app/entrypoint.sh"]
