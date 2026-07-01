FROM golang:1.26 AS build
WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 go build -trimpath -ldflags "-w -s" -o /pair-driver .

FROM alpine:3.24.1
COPY --from=build /pair-driver /pair-driver
RUN mkdir -p /run/docker/plugins
ENTRYPOINT ["/pair-driver"]
