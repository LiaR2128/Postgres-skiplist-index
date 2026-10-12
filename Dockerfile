FROM postgres:18.6

RUN apt-get update && \
    apt-get install -y --no-install-recommends build-essential postgresql-server-dev-18 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/pg_skiplist
COPY . .
RUN make && make install
