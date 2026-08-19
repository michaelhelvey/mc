default:
    RUST_LOG=info fab build --no-cache true --mode debug . && ./target/debug/tests
