# TODO

## HTTP

- [ ] write a buffered reader implementation
- [ ] use said reader to write a header iterator
- [ ] write a `http_response_body_read_all(buf, max)` function
- [ ] write a OpenSSL transport
- [ ] use all of the above to make some kind of example program that connects to an SSL host and
      gets a JSON response and parses it and prints some information about it (e.g.
      https://opencode.ai/zen/go/v1/models)
