# Build Stage
FROM fuzzers/aflplusplus:3.12c as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y cmake git autotools-dev texinfo gperf flex bison libltdl-dev

## Add source code to the build stage.
WORKDIR /
RUN git clone https://github.com/capuanob/dateutils.git
WORKDIR /dateutils
#RUN git checkout mayhem

## Build
RUN autoreconf -i
RUN CC=afl-clang-fast CXX=afl-clang-fast++ ./configure
RUN make -j$(nproc)

# Package Stage
FROM fuzzers/aflplusplus:3.12c

# Explicitly copy test binaries
COPY --from=builder /dateutils/src/dadd /
COPY --from=builder /dateutils/src/ddiff /
COPY --from=builder /dateutils/src/dgrep /
COPY --from=builder /dateutils/src/dconv /
COPY --from=builder /dateutils/src/dround /
COPY --from=builder /dateutils/src/dseq /
COPY --from=builder /dateutils/src/dzone /
COPY --from=builder /dateutils/src/dsort /
COPY --from=builder /dateutils/src/dtest /
COPY --from=builder /dateutils/src/strptime /

## Debugging corpus
RUN mkdir /tests && echo "2012-01-01" > /tests/seed

## Set up fuzzing!
ENTRYPOINT ["afl-fuzz", "-i", "/tests", "-o", "/out"]
CMD ["/dadd", "+4d"]
