// Example: Rust ↔ C++ shared memory interop
//
// Demonstrates creating a shared memory segment from Rust,
// writing data into it, then reading it back via a second handle —
// exactly the same pattern a C++ process could use on the other end.

use libsharedmemory::SharedMemory;

fn main() {
    let segment_name = "rustExample";
    let segment_size: usize = 256;

    // Writer: create the shared memory segment and copy a message in
    let writer = SharedMemory::create(segment_name, segment_size, true)
        .expect("Failed to create shared memory segment");

    let message = b"Hello from Rust!";
    writer.as_mut_slice()[..message.len()].copy_from_slice(message);
    println!("Wrote   : {}", std::str::from_utf8(message).unwrap());

    // Reader: open the same segment and read the bytes back
    let reader = SharedMemory::open(segment_name, segment_size, true)
        .expect("Failed to open shared memory segment");

    let received = &reader.as_slice()[..message.len()];
    println!("Received: {}", std::str::from_utf8(received).unwrap());

    assert_eq!(received, message, "Round-trip mismatch!");
    println!("OK");
}
