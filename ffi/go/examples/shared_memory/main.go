// Example: Go ↔ C++ shared memory interop
//
// Demonstrates creating a shared memory segment from Go,
// writing data into it, then reading it back via a second handle —
// exactly the same pattern a C++ (or Rust, Zig, C) process could use
// on the other end.
package main

import (
	"fmt"
	"log"

	lsm "libsharedmemory"
)

func main() {
	message := []byte("Hello from Go!")

	// Writer: create the shared memory segment and copy a message in
	writer, err := lsm.Create("goExample", 256, true)
	if err != nil {
		log.Fatalf("Failed to create shared memory: %v", err)
	}
	defer writer.Close()

	writer.Write(message)
	fmt.Printf("Wrote   : %s\n", message)

	// Reader: open the same segment and read the bytes back
	reader, err := lsm.Open("goExample", 256, true)
	if err != nil {
		log.Fatalf("Failed to open shared memory: %v", err)
	}
	defer reader.Close()

	received := reader.Data()[:len(message)]
	fmt.Printf("Received: %s\n", received)

	if string(received) != string(message) {
		log.Fatal("FAIL: round-trip mismatch!")
	}
	fmt.Println("OK")
}
