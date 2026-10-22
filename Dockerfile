# ==============================================================================
# Multi-stage Dockerfile for C++ Forex Exchange REST API Server
# Builds a lightweight, production-ready container compatible with Render & Docker
# ==============================================================================

# --- Stage 1: Build the C++20 binary ---
FROM gcc:13-bookworm AS builder

WORKDIR /build

# Copy required source files and headers
COPY httplib.h json.hpp server.cpp CMakeLists.txt ./
COPY src/ ./src/

# Compile with C++20, -O3 optimizations, static C++ runtime, and POSIX threads for Linux
RUN g++ -std=c++20 -O3 -static-libgcc -static-libstdc++ server.cpp -o forex_server -lpthread

# --- Stage 2: Minimal runtime image ---
FROM debian:bookworm-slim

WORKDIR /app

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Copy compiled executable from builder stage
COPY --from=builder /build/forex_server /app/forex_server
RUN chmod +x /app/forex_server

# Render dynamically assigns PORT (defaulting to 8080 locally)
ENV PORT=8080
EXPOSE 8080

CMD ["/app/forex_server"]
