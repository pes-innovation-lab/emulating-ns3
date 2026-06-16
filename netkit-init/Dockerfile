FROM alpine:latest
RUN apk add --no-cache curl jq iproute2-minimal ethtool
COPY init.sh /app/init.sh

CMD [ "/bin/sh", "-c", "/app/init.sh" ]
