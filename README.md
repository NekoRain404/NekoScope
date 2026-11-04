# NekoScope

NekoScope is an interactive H5 prototype for a Linux hardware information and diagnostics app. It uses simulated device and sensor data; it does not access the host hardware or run real stress and benchmark workloads.

## Run locally

Requirements: Node.js 20.19+ or 22.12+.

```sh
npm install
npm run dev
```

Open <http://localhost:4173/>. For a production bundle, run `npm run build` and serve the `dist/` directory.

## Features in this prototype

- System overview, hardware details, sensor table, stress-test lab, benchmark history and report center.
- Simulated telemetry, sensor filters and favorites, dark mode, simulated test progress and result comparison.
- Redacted-by-default JSON and standalone HTML report downloads, both marked as simulated data.
- Responsive desktop and mobile layouts.

## Verify

```sh
npm test
npm run build
```
