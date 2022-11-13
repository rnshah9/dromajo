FROM --platform=linux/amd64 ubuntu:22.04 as builder

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential cmake clang

COPY . /repo
WORKDIR /repo/build
RUN cmake ..
RUN make -j8
RUN clang++ /repo/mayhem/fuzz_json_parse_value.cpp -fsanitize=fuzzer,address -L /repo/build/ -ldromajo_cosim -I /repo/include/ -o /fuzz

FROM ubuntu:22.04 as package
COPY --from=builder /repo/build/dromajo /
COPY --from=builder /fuzz /fuzz_json_parse_value
