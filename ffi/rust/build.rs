// build.rs — compile the C++ wrapper (lsm_c.cpp) and link it into the Rust crate.
//
// The wrapper is a thin C-ABI shim around the header-only C++20 library,
// so we compile it as C++ and let `cc` handle the platform linker flags.

fn main() {
    let root = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../..");

    cc::Build::new()
        .cpp(true)
        .std("c++20")
        .file(root.join("example/lsm_c.cpp"))
        .include(root.join("include"))
        .include(root.join("example"))
        .compile("lsm_c");
}
