# Performance log

| Date       | Device     | Preset | Commit       | Scene          | Avg FPS | CPU (ms) | GPU (ms) | Notes               |
|------------|------------|:------:|:------------:|----------------|---------|----------|----------|---------------------|
| 2026-08-17 | RTX 4070 S | high   | `785fde09e2` | Bistro         | 98      | 10.0     | 10.0     | Radiance caching    |
|            |            | high   |              | Bistro (night) | 104     | 7.9      | 9.5      |                     |
|            |            | ultra  |              | Bistro         | 50      | 12.0     | 19.8     |                     |
|            |            | ultra  |              | Bistro (night) | 56      | 8.2      | 17.8     |                     |
|            | RX 9070 XT | high   |              | Bistro         | 87      | 5.9      | 10.9     |                     |
|            |            | high   |              | Bistro (night) | 91      | 4.7      | 10.4     |                     |
|            |            | ultra  |              | Bistro         | 44      | 6.7      | 22.0     |                     |
|            |            | ultra  |              | Bistro (night) | 48      | 4.7      | 20.0     |                     |
|            | Arc A310   | high   |              | Bistro         | 7.9     | 6.9      | 127.0    |                     |
|            |            | high   |              | Bistro (night) | 8.1     | 5.5      | 123.0    |                     |
|            |            | ultra  |              | Bistro         | 3.5     | 8.3      | 283.0    |                     |
|            |            | ultra  |              | Bistro (night) | 3.8     | 6.2      | 261.0    |                     |
| 2026-04-08 | RTX 4070 S | high   | `5797b34806` | Bistro         | 82      | 8.6      | 12.2     | Deferred RTSpecular |
|            |            | high   |              | Bistro (night) | 85      | 7.2      | 11.6     |                     |
|            |            | ultra  |              | Bistro         | 40      | 9.2      | 24.7     |                     |
|            |            | ultra  |              | Bistro (night) | 44      | 7.5      | 22.5     |                     |
|            | RX 9070 XT | high   |              | Bistro         | 78      | 6.0      | 12.0     |                     |
|            |            | high   |              | Bistro (night) | 82      | 4.8      | 11.5     |                     |
|            |            | ultra  |              | Bistro         | 37      | 6.8      | 24.6     |                     |
|            |            | ultra  |              | Bistro (night) | 41      | 4.8      | 22.7     |                     |
|            | Arc A310   | high   |              | Bistro         | 6.1     | 6.8      | 155.0    |                     |
|            |            | high   |              | Bistro (night) | 6.5     | 5.7      | 147.0    |                     |
|            |            | ultra  |              | Bistro         | 2.7     | 7.9      | 353.0    |                     |
|            |            | ultra  |              | Bistro (night) | 3.3     | 5.8      | 325.0    |                     |
| 2026-04-05 | RTX 4070 S | high   | `5b198fd69a` | Bistro         | 77      | 8.6      | 13.1     | None                |
|            |            | high   |              | Bistro (night) | 80      | 7.7      | 12.4     |                     |
|            |            | ultra  |              | Bistro         | 37      | 12.5     | 26.5     |                     |
|            |            | ultra  |              | Bistro (night) | 41      | 8.1      | 24.2     |                     |
|            | RX 9070 XT | high   |              | Bistro         | 71      | 5.6      | 13.5     |                     |
|            |            | high   |              | Bistro (night) | 73      | 4.5      | 13.1     |                     |
|            |            | ultra  |              | Bistro         | 34      | 6.4      | 28.5     |                     |
|            |            | ultra  |              | Bistro (night) | 37      | 4.9      | 26.2     |                     |
|            | Arc A310   | high   |              | Bistro         | 6.4     | 6.8      | 152.0    |                     |
|            |            | high   |              | Bistro (night) | 6.6     | 5.2      | 149.0    |                     |
|            |            | ultra  |              | Bistro         | 2.8     | 7.4      | 348      |                     |
|            |            | ultra  |              | Bistro (night) | 3.1     | 5.3      | 324      |                     |
| 2025-11-13 | RTX 4070 S | high   | `1d6ef0cd56` | Bistro         | 78      | 8.6      | 12.8     | Deferred RTDiffuse  |
|            |            | high   |              | Bistro (night) | 81      | 7.7      | 12.3     |                     |
|            |            | ultra  |              | Bistro         | 38      | 11.5     | 26.3     |                     |
|            |            | ultra  |              | Bistro (night) | 41      | 8.6      | 23.9     |                     |
|            | RX 9070 XT | high   |              | Bistro         | 72      | 5.8      | 13.2     |                     |
|            |            | high   |              | Bistro (night) | 73      | 4.7      | 12.9     |                     |
|            |            | ultra  |              | Bistro         | 33      | 6.7      | 28.5     |                     |
|            |            | ultra  |              | Bistro (night) | 36      | 4.8      | 25.7     |                     |
|            | Arc A310   | high   |              | Bistro         | 6.7     | 6.8      | 142.0    |                     |
|            |            | high   |              | Bistro (night) | 7.1     | 5.6      | 138.0    |                     |
|            |            | ultra  |              | Bistro         | 3.1     | 7.4      | 323.0    |                     |
|            |            | ultra  |              | Bistro (night) | 3.3     | 5.7      | 304.0    |                     |
| 2025-10-13 | RTX 4070 S | high   | `318a14ddd2` | Bistro         | 76      | 12.5     | 12.9     | Small ltree stack   |
|            |            | high   |              | Bistro (night) | 81      | 9.3      | 12.3     |                     |
|            |            | ultra  |              | Bistro         | 37      | 12.5     | 26.1     |                     |
|            |            | ultra  |              | Bistro (night) | 41      | 10.6     | 23.8     |                     |
|            | RX 9070 XT | high   |              | Bistro         | 65      | 5.8      | 14.8     |                     |
|            |            | high   |              | Bistro (night) | 67      | 4.7      | 14.6     |                     |
|            |            | ultra  |              | Bistro         | 29      | 6.8      | 33.0     |                     |
|            |            | ultra  |              | Bistro (night) | 30      | 5.0      | 30.4     |                     |
|            | Arc A310   | high   |              | Bistro         | 6       | 6.9      | 145.0    |                     |
|            |            | high   |              | Bistro (night) | 7       | 5.8      | 139.5    |                     |
|            |            | ultra  |              | Bistro         | 3       | 7.6      | 330.0    |                     |
|            |            | ultra  |              | Bistro (night) | 3.3     | 5.5      | 304.0    |                     |