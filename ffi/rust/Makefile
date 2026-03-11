.PHONY: setup build test example clean

setup:
	@command -v rustup >/dev/null 2>&1 || { echo "Error: rustup not found. Install it from https://rustup.rs"; exit 1; }
	rustup default stable
	@echo "Rust toolchain ready."

build:
	cargo build

test:
	cargo test

example: build
	cargo run --example shared_memory

clean:
	cargo clean
