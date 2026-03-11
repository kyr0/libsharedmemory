.PHONY: setup build test example clean

setup:
	@command -v zig >/dev/null 2>&1 && { echo "All good! Zig $$(zig version) is installed."; exit 0; }; \
	OS=$$(uname -s); \
	case $$OS in \
		Darwin) \
			echo "Installing zig via Homebrew..."; \
			command -v brew >/dev/null 2>&1 || { echo "Error: Homebrew not found. Install it from https://brew.sh"; exit 1; }; \
			brew install zig ;; \
		Linux) \
			if command -v snap >/dev/null 2>&1; then \
				echo "Installing zig via snap..."; \
				sudo snap install zig --classic --beta; \
			else \
				echo "Please install Zig manually: https://ziglang.org/download/"; \
				exit 1; \
			fi ;; \
		*) \
			echo "Please install Zig manually: https://ziglang.org/download/"; \
			exit 1 ;; \
	esac

build:
	zig build

test:
	zig build test

example: build
	zig build run

clean:
	rm -rf .zig-cache zig-out
