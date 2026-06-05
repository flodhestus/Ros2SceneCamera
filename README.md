# ROS2 Scene Camera

Win64 UE 5.7 plugin: HDR scene camera with compute-shader RGB8 export, CycloneDDS `sensor_msgs/Image`, and a live preview on Play.

Repo: [github.com/flodhestus/Ros2SceneCamera](https://github.com/flodhestus/Ros2SceneCamera)

## What it does

On Play, publishes and subscribes on `rt/sensor_image` and opens **Scene Camera**.

Works alongside either LiDAR plugin (different DDS topic).

## GPU pipeline

```
SceneCapture (SCS_FinalToneCurveHDR) → float RGBA render target [A/B]
  → Compute shader (16×16) → RGB8 readback → DDS sample [A/B]
  → SwapSamples → dds_write (async)
```

Two **PF_FloatRGBA** render targets and two DDS image buffers ping-pong via `SwapSamples()`. Capture runs on demand at `PublishRateHz`, not every engine frame.

### Render target settings

| Setting | Value |
|---------|--------|
| Format | `PF_FloatRGBA` / `RTF_RGBA32f` |
| Gamma | Linear (`TargetGamma=1`, `bForceLinearGamma`) |
| UAV | Enabled for compute read |
| Address | Clamp |

### Scene capture settings

| Setting | Value |
|---------|--------|
| `CaptureSource` | `SCS_FinalToneCurveHDR` |
| `bUseRayTracingIfEnabled` | true |
| `bCaptureEveryFrame` | false |
| Show flags | Post-processing + tone curve on |

## Performance

| Setting | Default | Per frame |
|---------|---------|-----------|
| Resolution | 1920×1080 | 6.2 MB RGB8 out |
| `PublishRateHz` | 20 | ~124 MB/s DDS payload |
| Buffers | 2 RT + 2 DDS | Overlap capture and publish |

Tips:

- Lower `PublishRateHz` if `FramesInFlight` hits 2 (publish back-pressure).
- Reduce resolution before rate if GPU bound.
- HDR float RTs use more VRAM than LDR; 1080p × 2 ≈ 32 MB float buffers.

## Quick start

1. Copy into `YourProject/Plugins/`.
2. Enable **ROS2 Scene Camera**.
3. Press Play.

## Related plugins

- [Lidar360GpuRayTracing](https://github.com/flodhestus/Lidar360GpuRayTracing) — D3D12 RT LiDAR  
- [Lidar360OptiX](https://github.com/flodhestus/Lidar360OptiX) — OptiX LiDAR  
