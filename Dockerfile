FROM debian:bookworm-slim

# Isolated robustness lab. No host network. Loopback only.
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ make python3 clang \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /lab
COPY . .

RUN make lab

# 24h unattended run. Engines send only to 127.0.0.1.
CMD ["python3", "controller/controller.py", "--hours", "24"]
