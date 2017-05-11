# LightMQTT

LightMQTT is a MQTT 3.1.1 client library written in pure C and designed to work
with both embedded and large systems.

## Features

* Fully asynchronous (no threads)
* Supports QoS 0, 1 and 2 messages
* No `malloc`'s. Pure C. (Only dependency is `string.h`)
* Handles large payloads in both directions
* Minimal memory requirements, even with large payloads (about 2 KB)

## License

See `LICENSE`.
