FROM golang:1.26 AS build
WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 go build -o /netkit-driver .

FROM alpine:3.24.1
COPY --from=build /netkit-driver /netkit-driver
RUN mkdir -p /run/docker/plugins
ENTRYPOINT ["/netkit-driver"]
