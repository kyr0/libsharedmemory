package lsm

import (
	"testing"
)

func TestRoundtrip(t *testing.T) {
	msg := []byte("Hello from Go!")

	writer, err := Create("go_test_rt", 256, true)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}
	defer writer.Close()

	writer.Write(msg)

	reader, err := Open("go_test_rt", 256, true)
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer reader.Close()

	got := reader.Data()[:len(msg)]
	if string(got) != string(msg) {
		t.Fatalf("roundtrip mismatch: got %q, want %q", got, msg)
	}
}

func TestSizeMatches(t *testing.T) {
	mem, err := Create("go_test_sz", 512, true)
	if err != nil {
		t.Fatalf("Create failed: %v", err)
	}
	defer mem.Close()

	if mem.Size() != 512 {
		t.Fatalf("size mismatch: got %d, want 512", mem.Size())
	}
}
