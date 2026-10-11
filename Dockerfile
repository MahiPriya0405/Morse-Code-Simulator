FROM gcc:latest

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends libssl-dev \
    && rm -rf /var/lib/apt/lists/*

COPY server.c database.c database.h morse_logic.c morse_logic.h password_hash.c password_hash.h sqlite3.c sqlite3.h ./

RUN gcc server.c database.c morse_logic.c password_hash.c sqlite3.c -o morse_server -lssl -lcrypto

EXPOSE 8080

CMD ["./morse_server"]