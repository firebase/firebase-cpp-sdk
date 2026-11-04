# Cloud Functions for Functions Integration Tests

This directory contains the Cloud Functions backend used by the Functions integration tests.

## Running Locally via Emulator

To run these functions locally using the Firebase emulator:

1. From this directory, run the startup orchestrator script:
   ```bash
   node start.js
   ```
   This will install dependencies, spin up the Cloud Functions emulator on port `5005`, and wait for it to be ready.

2. Run the integration test client app, specifying the emulator host:
   - Command-line flag: `--functions_emulator_host=localhost:5005`
   - Environment variable: `FUNCTIONS_EMULATOR_HOST=localhost:5005`

   *(Note: On Android, `localhost` or `127.0.0.1` will be automatically mapped to `10.0.2.2` to route requests to the host machine.)*
