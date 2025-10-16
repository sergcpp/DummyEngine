# Performance log

| Date       | Device     | Preset | Commit       | Scene          | Avg FPS | CPU (ms) | GPU (ms) | Notes             |
|------------|------------|:------:|:------------:|----------------|---------|----------|----------|-------------------|
| 2025-10-13 | RTX 2070   | high   | `318a14ddd2` | Bistro         | 29      | 17.7     | 34.4     | Small ltree stack |
|            |            | high   |              | Bistro (night) | 31      | 12.5     | 32.2     |                   |
|            |            | ultra  |              | Bistro         | 13      | 20.1     | 77.2     |                   |
|            |            | ultra  |              | Bistro (night) | 14      | 13.2     | 69.5     |                   |
|            | RX 9070 XT | high   |              | Bistro         | 65      | 5.8      | 14.8     |                   |
|            |            | high   |              | Bistro (night) | 67      | 4.7      | 14.6     |                   |
|            |            | ultra  |              | Bistro         | 29      | 6.8      | 33.0     |                   |
|            |            | ultra  |              | Bistro (night) | 30      | 5.0      | 30.4     |                   |
|            | Arc A310   | high   |              | Bistro         | 6       | 6.9      | 145.0    |                   |
|            |            | high   |              | Bistro (night) | 7       | 5.8      | 139.5    |                   |
|            |            | ultra  |              | Bistro         | 3       | 7.6      | 330.0    |                   |
|            |            | ultra  |              | Bistro (night) | 3.3     | 5.5      | 304.0    |                   |