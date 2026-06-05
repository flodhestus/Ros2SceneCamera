# Rebuilding ddsc.dll (Win64)

Headers in `include/dds/features.h` assume CycloneDDS was built **without**:

- Security (`ENABLE_SECURITY=OFF`)
- Lifespan QoS (`ENABLE_LIFESPAN=OFF`)
- TLS / OpenSSL TCP (`ENABLE_SSL=OFF`)
- Nested domain (`DDS_ALLOW_NESTED_DOMAIN` unset)
- Static library (`BUILD_SHARED_LIBS=ON`)
- QoS provider (`ENABLE_QOS_PROVIDER=OFF`)

Example:

```bat
cmake -S . -B build -DBUILD_SHARED_LIBS=ON ^
  -DENABLE_SECURITY=OFF -DENABLE_LIFESPAN=OFF -DENABLE_SSL=OFF ^
  -DENABLE_QOS_PROVIDER=OFF
cmake --build build --config Release
```

Copy `ddsc.dll` / `ddsc.lib` into `bin/Win64` and `lib/Win64`.
