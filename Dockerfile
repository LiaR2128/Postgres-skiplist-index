FROM postgres:18

RUN apt-get update && apt-get install -y \
    build-essential \
    postgresql-server-dev-18 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/pg_skiplist
COPY . .

# Compilar permitiendo estándares modernos de C sin abortar en warnings
RUN make CFLAGS="-O2 -std=gnu99" && make install