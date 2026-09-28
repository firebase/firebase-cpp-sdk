# Cloud Functions for Messaging Integration Tests

This directory contains the Cloud Functions backend used by the Messaging integration tests.

## Running Locally via Emulator

To run these functions locally using the Firebase emulator:

1. Navigate to the nested `functions/` directory and install dependencies:
   ```bash
   cd functions && npm install && cd ..
   ```

2. Start the Firebase emulator:
   ```bash
   npx firebase emulators:start --only functions --project functions-integration-test
   ```

3. Run the integration test client app, specifying the emulator host:
   - Command-line flag: `--functions_emulator_host=localhost:5005`
   - Environment variable: `FUNCTIONS_EMULATOR_HOST=localhost:5005`

   *(Note: On Android, `localhost` or `127.0.0.1` will be automatically mapped to `10.0.2.2` to route requests to the host machine.)*
