.PHONY: build test examples clean setup

build:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build

test: build
	ctest --test-dir build --output-on-failure

examples: build
	@echo "--- stream ---"  && ./build/example/example_stream
	@echo "--- queue ---"   && ./build/example/example_queue
	@echo "--- raw (C) ---" && ./build/example/example_raw

clean:
	rm -rf build

setup:
	@command -v cmake >/dev/null 2>&1 && { echo "All good! cmake is already installed."; exit 0; }; \
	OS=$$(uname -s); \
	case $$OS in \
		Darwin) \
			echo "Installing cmake via Homebrew..."; \
			command -v brew >/dev/null 2>&1 || { echo "Error: Homebrew not found. Install it from https://brew.sh"; exit 1; }; \
			brew install cmake ;; \
		Linux) \
			if command -v apt-get >/dev/null 2>&1; then \
				echo "Installing cmake via apt..."; \
				sudo apt-get update && sudo apt-get install -y cmake; \
			elif command -v dnf >/dev/null 2>&1; then \
				echo "Installing cmake via dnf..."; \
				sudo dnf install -y cmake; \
			elif command -v pacman >/dev/null 2>&1; then \
				echo "Installing cmake via pacman..."; \
				sudo pacman -S --noconfirm cmake; \
			else \
				echo "Could not detect package manager. Please install cmake manually:"; \
				echo "  https://cmake.org/download/"; \
				exit 1; \
			fi ;; \
		*) \
			echo "Please install cmake manually:"; \
			echo "  - Windows: https://cmake.org/download/ or 'winget install Kitware.CMake'"; \
			echo "  - Other:   https://cmake.org/download/"; \
			exit 1 ;; \
	esac
