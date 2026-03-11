.PHONY: setup build test example clean lib

REPO_ROOT := $(shell cd ../.. && pwd)
LIB_DIR   := $(CURDIR)/lib

setup:
	@command -v go >/dev/null 2>&1 && { echo "All good! Go $$(go version | awk '{print $$3}') is installed."; exit 0; }; \
	OS=$$(uname -s); \
	case $$OS in \
		Darwin) \
			echo "Installing go via Homebrew..."; \
			command -v brew >/dev/null 2>&1 || { echo "Error: Homebrew not found. Install it from https://brew.sh"; exit 1; }; \
			brew install go ;; \
		Linux) \
			if command -v apt-get >/dev/null 2>&1; then \
				echo "Installing golang via apt..."; \
				sudo apt-get update && sudo apt-get install -y golang; \
			elif command -v dnf >/dev/null 2>&1; then \
				echo "Installing golang via dnf..."; \
				sudo dnf install -y golang; \
			else \
				echo "Please install Go manually: https://go.dev/dl/"; \
				exit 1; \
			fi ;; \
		*) \
			echo "Please install Go manually: https://go.dev/dl/"; \
			exit 1 ;; \
	esac

lib:
	@mkdir -p $(LIB_DIR)
	$(CXX) -std=c++20 -c -fPIC -I$(REPO_ROOT)/include -I$(REPO_ROOT)/example \
		$(REPO_ROOT)/example/lsm_c.cpp -o $(LIB_DIR)/lsm_c.o
	ar rcs $(LIB_DIR)/liblsm_c.a $(LIB_DIR)/lsm_c.o

build: lib
	go build ./...

test: lib
	go test -v ./...

example: lib
	go run ./examples/shared_memory

clean:
	rm -rf $(LIB_DIR)
	go clean -cache 2>/dev/null || true
